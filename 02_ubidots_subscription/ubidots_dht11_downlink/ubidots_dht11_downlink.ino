#include "UbidotsEsp32Mqtt.h"
#include <TFT_eSPI.h>
#include <SPI.h>
#include <DHT.h>

TFT_eSPI tft = TFT_eSPI(135, 240); // Pantalla del TTGO T-Display
#define TFT_BL 4 // Pin de backlight

#define DHTPIN 25    // Pin de datos del DHT11
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);


// Parametros de Ubidots
const char *UBIDOTS_TOKEN  = "TU_TOKEN_AQUI";
const char *WIFI_SSID      = "TU_WIFI_AQUI";
const char *WIFI_PASS      = "TU_PASSWORD_AQUI";
const char *DEVICE_LABEL   = "gilbert_ttgo";
const char *TEMP_LABEL     = "temperatura";
const char *HUM_LABEL      = "humedad";
const char *CONTROL_LABEL  = "control"; // Variable a la que nos suscribimos (Slider/Switch en el dashboard)

const unsigned long PUBLISH_FREQUENCY = 2000; // el DHT11 solo acepta 1 lectura/segundo, 2s da margen

Ubidots ubidots(UBIDOTS_TOKEN);
unsigned long timer;
unsigned long lastReceived = 0; // marca de tiempo del ultimo dato recibido de Ubidots


// Callback: se dispara solo cuando "control" cambia de valor en Ubidots
void callback(char *topic, byte *payload, unsigned int length) {
  // El payload del topico /lv es texto plano, no JSON
  String valorTexto = "";
  for (unsigned int i = 0; i < length; i++) {
    valorTexto += (char)payload[i];
  }
  float valorRecibido = valorTexto.toFloat();

  Serial.print("Valor recibido de Ubidots: ");
  Serial.println(valorRecibido);

  lastReceived = millis();

  // Mostrar el valor recibido en la columna izquierda
  tft.fillRect(5, 45, 105, 25, TFT_BLACK); // borra el valor anterior
  tft.setCursor(5, 45);
  tft.setTextSize(3);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.println(valorRecibido, 1);
}

void setup() {
  Serial.begin(115200);
  delay(200);

  dht.begin();

  // Iniciar pantalla
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);

  // Titulo
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(5, 5);
  tft.println("DHT11 + Control");

  // Linea divisoria entre columnas
  tft.drawFastVLine(120, 28, 100, TFT_DARKGREY);

  // Etiquetas columna izquierda (recibido de Ubidots)
  tft.setTextSize(1);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setCursor(5, 30);
  tft.println("Recibido:");

  // Etiquetas columna derecha (DHT11)
  tft.setCursor(130, 30);
  tft.println("Temp:");
  tft.setCursor(130, 72);
  tft.println("Humedad:");

  ubidots.setDebug(true); // ver logs de conexion, publicacion y suscripcion

  ubidots.connectToWifi(WIFI_SSID, WIFI_PASS);
  ubidots.setCallback(callback);
  ubidots.setup();
  ubidots.reconnect();
  ubidots.subscribeLastValue(DEVICE_LABEL, CONTROL_LABEL);

  timer = millis();
  Serial.println("Setup completo. Publicando DHT11 y escuchando 'control'...");
}

void loop() {
  if (!ubidots.connected()) {
    ubidots.reconnect();
    ubidots.subscribeLastValue(DEVICE_LABEL, CONTROL_LABEL); // resuscribir tras reconectar
  }

  if (millis() - timer > PUBLISH_FREQUENCY) {
    timer = millis();

    float humedad = dht.readHumidity();
    float temperatura = dht.readTemperature(); // Celsius

    // El DHT11 a veces falla la lectura y devuelve NaN
    if (isnan(humedad) || isnan(temperatura)) {
      Serial.println("Error leyendo el DHT11");

      tft.fillRect(130, 45, 105, 60, TFT_BLACK);
      tft.setCursor(130, 45);
      tft.setTextSize(2);
      tft.setTextColor(TFT_RED, TFT_BLACK);
      tft.println("Error");
    } else {
      ubidots.add(TEMP_LABEL, temperatura);
      ubidots.add(HUM_LABEL, humedad);
      bool sent = ubidots.publish(DEVICE_LABEL);

      Serial.print("Conectado a Ubidots: ");
      Serial.println(ubidots.connected() ? "SI" : "NO");

      if (sent) {
        Serial.print("Publicado OK -> Temp: ");
        Serial.print(temperatura);
        Serial.print(" C, Humedad: ");
        Serial.print(humedad);
        Serial.println(" %");
      } else {
        Serial.println("FALLO al publicar (revisar token / conexion MQTT arriba)");
      }

      // Mostrar temperatura (columna derecha, arriba)
      tft.fillRect(130, 45, 105, 20, TFT_BLACK);
      tft.setCursor(130, 45);
      tft.setTextSize(2);
      tft.setTextColor(sent ? TFT_GREEN : TFT_RED, TFT_BLACK);
      tft.print(temperatura, 1);
      tft.println(" C");

      // Mostrar humedad (columna derecha, abajo)
      tft.fillRect(130, 87, 105, 20, TFT_BLACK);
      tft.setCursor(130, 87);
      tft.setTextColor(sent ? TFT_GREEN : TFT_RED, TFT_BLACK);
      tft.print(humedad, 1);
      tft.println(" %");
    }
  }

  // Columna izquierda, abajo: hace cuantos segundos llego el ultimo dato de Ubidots
  tft.fillRect(5, 110, 105, 20, TFT_BLACK);
  tft.setCursor(5, 110);
  tft.setTextSize(1);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.print("Hace ");
  tft.print((millis() - lastReceived) / 1000);
  tft.println("s");

  ubidots.loop();
}
