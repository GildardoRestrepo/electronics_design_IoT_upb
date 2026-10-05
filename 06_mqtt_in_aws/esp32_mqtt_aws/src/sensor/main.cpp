/*
 * Practica #6 - Diseño Electronico (IoT / Sistemas Embebidos)
 * Firmware "sensor" (cliente 1 de la arquitectura).
 *
 * ESP32 TTGO T-Display + DHT11 publicando a un broker Mosquitto que corre en
 * una instancia EC2 de AWS, es decir, en Internet y no en la red local.
 *
 *   Publica   : temperatura, humedad, sw (boton) y estado (LWT)
 *   Se suscribe: led1 y led2 (dos indicadores en pantalla)
 */

#include <Arduino.h>
#include <DHT.h>
#include <TFT_eSPI.h>
#include <SPI.h>

#include "secrets.h"
#include "topicos.h"
#include "conexion.h"

// ======================================================
// CONFIGURACION FISICA (igual que las practicas anteriores)
// ======================================================
TFT_eSPI tft = TFT_eSPI(135, 240);

#define DHTPIN 25
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

#define BTN_SW 35 // Boton derecho del T-Display (pull-up en placa: reposo = HIGH)

// ======================================================
// ESTADO
// ======================================================
float temperatura = 0.0;
float humedad = 0.0;
bool sensorOk = false;

int led1 = 0; // ordenes recibidas desde el dashboard
int led2 = 0;

bool swPulsado = false; // estado ESTABLE del boton (ya sin rebote)

bool refrescarPantalla = true;

// ======================================================
// TIEMPOS (loop no bloqueante)
// ======================================================
unsigned long tiempoAnterior = 0;
const unsigned long PUBLISH_FREQUENCY = 5000; // el DHT11 admite 1 lectura/s como maximo

bool lecturaBotonAnterior = false;
unsigned long cambioBoton = 0;
const unsigned long ANTIRREBOTE = 30;

// ======================================================
// PANTALLA
// ======================================================
void dibujarLed(int x, int y, int encendido, uint16_t color, const char *nombre)
{
  tft.fillCircle(x, y, 13, encendido ? color : TFT_DARKGREY);
  tft.setTextSize(1);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setCursor(x - 9, y + 18);
  tft.print(nombre);
}

void mostrarDatos()
{
  tft.fillScreen(TFT_BLACK);
  tft.setTextSize(2);

  if (sensorOk)
  {
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setCursor(8, 8);
    tft.print("T: ");
    tft.print(temperatura, 1);
    tft.print(" C");

    tft.setCursor(8, 34);
    tft.print("H: ");
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
  }

  // Indicadores controlados desde el dashboard en AWS
  dibujarLed(180, 20, led1, TFT_GREEN, "LED1");
  dibujarLed(218, 20, led2, TFT_BLUE, "LED2");

  // ---- Barra de estado ----
  tft.setTextSize(1);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setCursor(8, 66);
  tft.print("Broker: ");
  tft.print(MQTT_BROKER);

  tft.setCursor(8, 80);
  tft.print("MQTT: ");
  tft.setTextColor(mqtt.connected() ? TFT_GREEN : TFT_RED, TFT_BLACK);
  tft.print(mqttMotivo());

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setCursor(8, 94);
  tft.print("IP local: ");
  tft.print(ipLocal());

  tft.setCursor(8, 112);
  tft.print("SW: ");
  tft.setTextColor(swPulsado ? TFT_YELLOW : TFT_DARKGREY, TFT_BLACK);
  tft.print(swPulsado ? "PULSADO" : "libre");
}

// ======================================================
// MQTT
// ======================================================
int aBinario(const String &m)
{
  // Node-RED envia "1"/"0"; se aceptan tambien true/false y ON/OFF por si el
  // mensaje viene de otra herramienta (MQTT Explorer, mosquitto_pub...).
  return (m == "1" || m == "true" || m == "ON" || m == "on") ? 1 : 0;
}

void callback(char *topic, byte *payload, unsigned int length)
{
  String mensaje;
  for (unsigned int i = 0; i < length; i++)
  {
    mensaje += (char)payload[i];
  }
  String t = String(topic);

  Serial.print("MQTT recibido [");
  Serial.print(t);
  Serial.print("] -> ");
  Serial.println(mensaje);

  if (t == TOPIC_LED1)
  {
    led1 = aBinario(mensaje);
    refrescarPantalla = true;
  }
  else if (t == TOPIC_LED2)
  {
    led2 = aBinario(mensaje);
    refrescarPantalla = true;
  }
}

void publicarSw()
{
  // Retenido: el sw es un ESTADO (pulsado/libre), no un evento. Quien se
  // suscriba despues debe conocer su valor actual sin esperar a que cambie.
  mqtt.publish(TOPIC_SW, swPulsado ? "1" : "0", true);
}

void alConectar()
{
  // QoS 1 para las ordenes: el broker reintenta la entrega hasta recibir
  // confirmacion. Como Node-RED las publica RETENIDAS, al (re)conectar el
  // ESP32 recibe enseguida el ultimo estado pedido de led1 y led2.
  mqtt.subscribe(TOPIC_LED1, 1);
  mqtt.subscribe(TOPIC_LED2, 1);
  Serial.println("Suscrito a led1 y led2");

  publicarSw(); // sincroniza el valor retenido con el estado real del boton
  refrescarPantalla = true;
}

// ======================================================
// SENSOR
// ======================================================
void leerYPublicar()
{
  float t = dht.readTemperature();
  float h = dht.readHumidity();

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
  mqtt.publish(TOPIC_TEMP, buffer, true);
  dtostrf(humedad, 1, 1, buffer);
  mqtt.publish(TOPIC_HUM, buffer, true);

  Serial.print("Publicado -> T: ");
  Serial.print(temperatura, 1);
  Serial.print(" C, H: ");
  Serial.print(humedad, 1);
  Serial.println(" %");

  refrescarPantalla = true;
}

// ======================================================
// BOTON (sw)
// ======================================================
// Se publica en cada FLANCO (pulsar -> "1", soltar -> "0") y no mientras se
// mantiene pulsado. Corrige la limitacion de la practica 05, donde el boton
// repetia el mensaje cada 400 ms si se dejaba presionado.
void revisarBoton()
{
  bool lectura = (digitalRead(BTN_SW) == LOW);

  if (lectura != lecturaBotonAnterior)
  {
    cambioBoton = millis(); // la señal se movio: reiniciar la espera
    lecturaBotonAnterior = lectura;
  }

  if (millis() - cambioBoton > ANTIRREBOTE && lectura != swPulsado)
  {
    swPulsado = lectura; // lleva ANTIRREBOTE ms estable: es un cambio real
    Serial.print("SW -> ");
    Serial.println(swPulsado ? "pulsado" : "libre");
    if (mqtt.connected())
    {
      publicarSw();
    }
    refrescarPantalla = true;
  }
}

// ======================================================
// SETUP / LOOP
// ======================================================
void setup()
{
  Serial.begin(115200);

  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);

  tft.init();
  tft.setRotation(1); // horizontal: 240x135
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(10, 40);
  tft.print("Practica 06");
  tft.setTextSize(1);
  tft.setCursor(10, 70);
  tft.print("MQTT en AWS - sensor");

  pinMode(BTN_SW, INPUT);
  dht.begin();

  conexionIniciar("esp32-sensor", TOPIC_ESTADO, alConectar, callback);
  mostrarDatos();
}

void loop()
{
  if (conexionMantener())
  {
    refrescarPantalla = true;
  }

  revisarBoton();

  unsigned long ahora = millis();
  if (ahora - tiempoAnterior >= PUBLISH_FREQUENCY)
  {
    tiempoAnterior = ahora;
    if (mqtt.connected())
    {
      leerYPublicar();
    }
  }

  if (refrescarPantalla)
  {
    refrescarPantalla = false;
    mostrarDatos();
  }
}
