#include "UbidotsEsp32Mqtt.h"
#include <math.h>
#include <TFT_eSPI.h>
#include <SPI.h>

TFT_eSPI tft = TFT_eSPI(135, 240); // Pantalla del TTGO T-Display
#define TFT_BL 4 // Pin de backlight


// Parametros de Ubidots
const char *UBIDOTS_TOKEN  = "TU_TOKEN_AQUI";
const char *WIFI_SSID      = "TU_WIFI_AQUI";
const char *WIFI_PASS      = "TU_PASSWORD_AQUI";
const char *DEVICE_LABEL   = "gilbert_ttgo";
const char *VARIABLE_LABEL = "wave_test";

const unsigned long PUBLISH_FREQUENCY = 2000;


// Onda de prueba
const float AMPLITUDE = 25.0;
const float OFFSET    = 0;
const float PERIOD_S  = 30.0;

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

  // Iniciar pantalla
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(10, 10);
  tft.println("Ubidots MQTT");
  tft.setTextSize(1);
  tft.setCursor(10, 40);
  tft.println("Valor enviado:");

  ubidots.setDebug(true); // ACTIVADO: para ver logs detallados de la conexion MQTT

  ubidots.connectToWifi(WIFI_SSID, WIFI_PASS);
  ubidots.setCallback(callback);
  ubidots.setup();
  ubidots.reconnect();

  timer = millis();
  Serial.println("Setup completo. Publicando onda seno cada 2 segundos...");
}

void loop() {
  if (!ubidots.connected()) {
    ubidots.reconnect();
  }

  if (millis() - timer > PUBLISH_FREQUENCY) {
    timer = millis();

    float t = millis() / 1000.0;
    float value = OFFSET + AMPLITUDE * sin(2 * PI * t / PERIOD_S);

    ubidots.add(VARIABLE_LABEL, value);
    bool sent = ubidots.publish(DEVICE_LABEL);

    Serial.print("Conectado a Ubidots: ");
    Serial.println(ubidots.connected() ? "SI" : "NO");

    if (sent) {
      Serial.print("Publicado OK -> ");
      Serial.print(VARIABLE_LABEL);
      Serial.print(": ");
      Serial.println(value);
    } else {
      Serial.println("FALLO al publicar (revisar token / conexion MQTT arriba)");
    }

    // Mostrar el valor en la pantalla (verde si se publico bien, rojo si fallo)
    tft.fillRect(10, 60, 220, 30, TFT_BLACK); // borra el valor anterior
    tft.setCursor(10, 60);
    tft.setTextSize(3);
    tft.setTextColor(sent ? TFT_GREEN : TFT_RED, TFT_BLACK);
    tft.println(value, 2); // 2 decimales
  }

  ubidots.loop();
}
