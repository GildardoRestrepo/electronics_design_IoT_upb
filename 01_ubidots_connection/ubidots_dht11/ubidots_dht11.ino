#include "UbidotsEsp32Mqtt.h"
#include <math.h>
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

const unsigned long PUBLISH_FREQUENCY = 2000; // el DHT11 solo acepta 1 lectura/segundo, 2s da margen

Ubidots ubidots(UBIDOTS_TOKEN);
unsigned long timer;


//Callback (lo pide la libreria)
void callback(char *topic, byte *payload, unsigned int length) {
  Serial.print("Mensaje recibido [");
  Serial.print(topic);
  Serial.print("] ");
  for (unsigned int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
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
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(10, 10);
  tft.println("DHT11 + Ubidots");
  tft.setTextSize(1);
  tft.setCursor(10, 40);
  tft.println("Temperatura:");
  tft.setCursor(10, 90);
  tft.println("Humedad:");

  ubidots.setDebug(true); // ACTIVADO: para ver logs detallados de la conexion MQTT

  ubidots.connectToWifi(WIFI_SSID, WIFI_PASS);
  ubidots.setCallback(callback);
  ubidots.setup();
  ubidots.reconnect();

  timer = millis();
  Serial.println("Setup completo. Leyendo DHT11 cada 2 segundos...");
}

void loop() {
  if (!ubidots.connected()) {
    ubidots.reconnect();
  }

  if (millis() - timer > PUBLISH_FREQUENCY) {
    timer = millis();

    float humedad = dht.readHumidity();
    float temperatura = dht.readTemperature(); // Celsius

    // El DHT11 a veces falla la lectura y devuelve NaN
    if (isnan(humedad) || isnan(temperatura)) {
      Serial.println("Error leyendo el DHT11");

      tft.fillRect(10, 55, 220, 60, TFT_BLACK);
      tft.setCursor(10, 60);
      tft.setTextSize(2);
      tft.setTextColor(TFT_RED, TFT_BLACK);
      tft.println("Error sensor");
      return; // no publicar datos invalidos
    }

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

    // Mostrar temperatura en pantalla (verde si se publico bien, rojo si fallo)
    tft.fillRect(10, 60, 220, 25, TFT_BLACK); // borra el valor anterior
    tft.setCursor(10, 60);
    tft.setTextSize(2);
    tft.setTextColor(sent ? TFT_GREEN : TFT_RED, TFT_BLACK);
    tft.print(temperatura, 1);
    tft.println(" C");

    // Mostrar humedad en pantalla
    tft.fillRect(10, 110, 220, 25, TFT_BLACK); // borra el valor anterior
    tft.setCursor(10, 110);
    tft.setTextColor(sent ? TFT_GREEN : TFT_RED, TFT_BLACK);
    tft.print(humedad, 1);
    tft.println(" %");
  }

  ubidots.loop();
}
