/*
 * Practica #5 - Diseño Electronico (IoT / Sistemas Embebidos)
 *
 * Monitoreo de temperatura y humedad con ESP32 (TTGO T-Display) + DHT11,
 * publicando a un broker MQTT LOCAL (Mosquitto).
 */

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <TFT_eSPI.h>
#include <SPI.h>

#include "secrets.h" // WIFI_SSID, WIFI_PASSWORD, MQTT_BROKER, MQTT_PORT

// ======================================================
// CONFIGURACION FISICA (igual que las practicas 01-03)
// ======================================================
TFT_eSPI tft = TFT_eSPI(135, 240); // Pantalla del TTGO T-Display

#define DHTPIN 25 // Pin de datos del DHT11
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

#define BTN_PANICO 35 // Boton derecho del T-Display (tiene pull-up en placa)

// ======================================================
// ESQUEMA DE TOPICOS
// ======================================================
// Ubidots imponia /v1.6/devices/<label>/<variable>. Aqui somos libres, asi
// que se usa una jerarquia propia <usuario>/<dispositivo>/<señal>, que permite
// suscribirse a todo un dispositivo con el comodin "gilbert/ttgo/#".
//
// Se usa un topico por variable en vez de un unico JSON: asi cada topico se
// conecta directo a un widget de Node-RED sin nodos de parseo intermedios.
// La alternativa (un JSON con todas las variables) ahorra ancho de banda y es
// mas realista en produccion, pero pide un nodo "json" y un "change" por cada
// widget del dashboard.
const char *TOPIC_TEMP = "gilbert/ttgo/temperatura";
const char *TOPIC_HUM = "gilbert/ttgo/humedad";
const char *TOPIC_ESTADO = "gilbert/ttgo/estado";
const char *TOPIC_LED = "gilbert/ttgo/control/led";  // dashboard -> dispositivo
const char *TOPIC_PANICO = "gilbert/alertas/panico"; // uno a muchos

// ======================================================
// OBJETOS GLOBALES
// ======================================================
WiFiClient espClient;
PubSubClient client(espClient);
String clientId;

// ======================================================
// VARIABLES DE ESTADO
// ======================================================
float temperatura = 0.0;
float humedad = 0.0;
bool sensorOk = false;

int ledEstado = 0;         // recibido desde el dashboard
bool alertaPanico = false; // hay una alerta en pantalla ahora mismo
String origenAlerta = "";

bool refrescarPantalla = true; // para no redibujar en cada vuelta del loop

// ======================================================
// CONTROL DE TIEMPOS (loop no bloqueante con millis())
// ======================================================
unsigned long tiempoAnterior = 0;
const unsigned long PUBLISH_FREQUENCY = 5000; // el DHT11 solo acepta 1 lectura/segundo

unsigned long ultimoIntentoWiFi = 0;
const unsigned long INTERVALO_WIFI = 15000;

unsigned long ultimoIntentoMQTT = 0;
const unsigned long INTERVALO_MQTT = 5000;

unsigned long inicioAlerta = 0;
const unsigned long DURACION_ALERTA = 4000; // cuanto se queda la alerta en pantalla

unsigned long ultimoBoton = 0;
const unsigned long ANTIRREBOTE = 400;

// ======================================================
// PANTALLA: vista normal
// ======================================================
void mostrarDatos()
{
  tft.fillScreen(TFT_BLACK);
  tft.setTextSize(2);

  if (sensorOk)
  {
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setCursor(8, 8);
    tft.print("Temp: ");
    tft.print(temperatura, 1);
    tft.print(" C");

    tft.setCursor(8, 34);
    tft.print("Hum:  ");
    tft.print(humedad, 1);
    tft.print(" %");
  }
  else
  {
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.setCursor(8, 8);
    tft.print("Error DHT11");
    tft.setTextSize(1);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setCursor(8, 38);
    tft.print("Revise el sensor (GPIO 25)");
    tft.setTextSize(2);
  }

  // Indicador controlado desde el dashboard (topico control/led)
  tft.fillCircle(210, 24, 16, ledEstado ? TFT_GREEN : TFT_DARKGREY);

  // ---- Barra de estado ----
  tft.setTextSize(1);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);

  tft.setCursor(8, 66);
  tft.print("Broker: ");
  tft.print(MQTT_BROKER);

  tft.setCursor(8, 80);
  tft.print("MQTT: ");
  if (client.connected())
  {
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.print("conectado");
  }
  else
  {
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.print("desconectado");
  }

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setCursor(8, 94);
  tft.print("IP: ");
  tft.print(WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : String("sin WiFi"));

  tft.setCursor(8, 112);
  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.print("Boton derecho = panico");
}

// ======================================================
// PANTALLA: alerta de panico recibida
// ======================================================
void mostrarAlerta()
{
  tft.fillScreen(TFT_RED);
  tft.setTextColor(TFT_WHITE, TFT_RED);

  tft.setTextSize(3);
  tft.setCursor(20, 30);
  tft.print("PANICO");

  tft.setTextSize(1);
  tft.setCursor(20, 75);
  tft.print("Origen: ");
  tft.print(origenAlerta);
}

// ======================================================
// CALLBACK MQTT
// ======================================================
void callback(char *topic, byte *payload, unsigned int length)
{
  String mensaje = "";
  for (unsigned int i = 0; i < length; i++)
  {
    mensaje += (char)payload[i];
  }
  String topicRecibido = String(topic);

  Serial.print("\nMQTT recibido [");
  Serial.print(topicRecibido);
  Serial.print("] -> ");
  Serial.println(mensaje);

  if (topicRecibido == TOPIC_LED)
  {
    // Node-RED envia "0"/"1" (o "true"/"false" segun el widget)
    ledEstado = (mensaje == "1" || mensaje == "true" || mensaje == "ON") ? 1 : 0;
    refrescarPantalla = true;
  }
  else if (topicRecibido == TOPIC_PANICO)
  {
    // Llega tanto si la alerta la lanzo el dashboard, otro dispositivo, o
    // este mismo ESP32: el broker reparte la misma copia a TODOS los
    // suscriptores. Eso es exactamente el "uno a muchos" de MQTT.
    origenAlerta = mensaje;
    alertaPanico = true;
    inicioAlerta = millis();
    mostrarAlerta();
  }
}

// ======================================================
// CONEXION WIFI (no bloqueante: un intento por llamada)
// ======================================================
void conectarWiFi()
{
  Serial.print("\nConectando a WiFi: ");
  Serial.println(WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  // Espera acotada: si no entra en 10 s se sale y el loop reintentara.
  unsigned long inicio = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - inicio < 10000)
  {
    delay(250);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.print("\nWiFi conectado. IP: ");
    Serial.println(WiFi.localIP());
  }
  else
  {
    Serial.println("\nNo se pudo conectar al WiFi, se reintentara.");
  }
  refrescarPantalla = true;
}

// ======================================================
// CONEXION MQTT (con Last Will and Testament)
// ======================================================
bool conectarMQTT()
{
  Serial.print("Conectando al broker ");
  Serial.print(MQTT_BROKER);
  Serial.print(":");
  Serial.print(MQTT_PORT);
  Serial.print(" ... ");

  // El LWT se declara AL CONECTAR, no al desconectar: es un mensaje que se
  // deja "en custodia" del broker y que este publica solo si el cliente
  // desaparece sin despedirse (cable, bateria, caida de WiFi). Es la unica
  // forma de que el dashboard distinguga "0 grados" de "el sensor esta muerto".
  //
  // Usuario y contraseña van en NULL porque el broker esta con
  // allow_anonymous true.
  bool ok = client.connect(
      clientId.c_str(), // client id
      NULL,             // usuario
      NULL,             // contraseña
      TOPIC_ESTADO,     // will topic
      1,                // will QoS
      true,             // will retained
      "offline");       // will message

  if (ok)
  {
    Serial.println("conectado.");

    // Retained: el broker guarda este valor y se lo entrega de inmediato a
    // cualquiera que se suscriba despues. Sin esto, un dashboard recien
    // abierto se quedaria en blanco hasta la siguiente publicacion.
    client.publish(TOPIC_ESTADO, "online", true);

    client.subscribe(TOPIC_LED);
    client.subscribe(TOPIC_PANICO);
    Serial.println("Suscrito a control/led y alertas/panico");
  }
  else
  {
    Serial.print("fallo, rc=");
    Serial.println(client.state());
  }

  refrescarPantalla = true;
  return ok;
}

// ======================================================
// LECTURA Y PUBLICACION DEL DHT11
// ======================================================
void leerYPublicar()
{
  float t = dht.readTemperature();
  float h = dht.readHumidity();

  // Si el sensor falla devuelve NaN. No se publican datos invalidos.
  if (isnan(t) || isnan(h))
  {
    sensorOk = false;
    Serial.println("Lectura invalida del DHT11, no se publica.");
    refrescarPantalla = true;
    return;
  }

  temperatura = t;
  humedad = h;
  sensorOk = true;

  char buffer[16];

  dtostrf(temperatura, 1, 1, buffer);
  client.publish(TOPIC_TEMP, buffer, true); // retained

  dtostrf(humedad, 1, 1, buffer);
  client.publish(TOPIC_HUM, buffer, true); // retained

  Serial.print("Publicado -> temp: ");
  Serial.print(temperatura, 1);
  Serial.print(" C, hum: ");
  Serial.print(humedad, 1);
  Serial.println(" %");

  refrescarPantalla = true;
}

// ======================================================
// BOTON DE PANICO
// ======================================================
void revisarBoton()
{
  // El boton del T-Display lleva pull-up en la placa: en reposo lee HIGH.
  if (digitalRead(BTN_PANICO) == LOW && millis() - ultimoBoton > ANTIRREBOTE)
  {
    ultimoBoton = millis();

    if (client.connected())
    {
      // No retained: una alerta es un evento puntual. Si fuera retained, todo
      // cliente que se conectara despues recibiria una alarma ya pasada.
      client.publish(TOPIC_PANICO, "ESP32-TTGO");
      Serial.println("Boton de panico pulsado -> publicado en gilbert/alertas/panico");
    }
    else
    {
      Serial.println("Boton pulsado pero sin conexion MQTT.");
    }
  }
}

// ======================================================
// SETUP
// ======================================================
void setup()
{
  Serial.begin(115200);

  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH); // encender el backlight

  tft.init();
  tft.setRotation(1); // horizontal: 240x135
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(10, 40);
  tft.print("Practica 05");
  tft.setTextSize(1);
  tft.setCursor(10, 70);
  tft.print("MQTT local + Node-RED");

  pinMode(BTN_PANICO, INPUT);
  dht.begin();

  // Client id unico a partir de la MAC: si dos clientes usan el mismo id, el
  // broker expulsa al anterior y quedan reconectandose en bucle.
  clientId = "esp32-ttgo-" + String((uint32_t)ESP.getEfuseMac(), HEX);
  Serial.print("\nClient ID MQTT: ");
  Serial.println(clientId);

  conectarWiFi();

  client.setServer(MQTT_BROKER, MQTT_PORT);
  client.setCallback(callback);
  if (WiFi.status() == WL_CONNECTED)
  {
    conectarMQTT();
  }

  mostrarDatos();
}

// ======================================================
// LOOP
// ======================================================
void loop()
{
  unsigned long ahora = millis();

  // ---- WiFi ----
  if (WiFi.status() != WL_CONNECTED)
  {
    if (ahora - ultimoIntentoWiFi >= INTERVALO_WIFI)
    {
      ultimoIntentoWiFi = ahora;
      conectarWiFi();
    }
  }

  // ---- MQTT ----
  if (WiFi.status() == WL_CONNECTED && !client.connected())
  {
    if (ahora - ultimoIntentoMQTT >= INTERVALO_MQTT)
    {
      ultimoIntentoMQTT = ahora;
      conectarMQTT();
    }
  }
  client.loop(); // procesa los mensajes entrantes y mantiene viva la conexion

  // ---- Boton de panico ----
  revisarBoton();

  // ---- Publicacion periodica ----
  if (ahora - tiempoAnterior >= PUBLISH_FREQUENCY)
  {
    tiempoAnterior = ahora;
    if (client.connected())
    {
      leerYPublicar();
    }
  }

  // ---- Pantalla ----
  if (alertaPanico)
  {
    if (ahora - inicioAlerta >= DURACION_ALERTA)
    {
      alertaPanico = false;
      refrescarPantalla = true;
    }
  }
  else if (refrescarPantalla)
  {
    refrescarPantalla = false;
    mostrarDatos();
  }
}
