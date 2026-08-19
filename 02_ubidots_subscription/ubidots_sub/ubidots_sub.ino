#include "UbidotsEsp32Mqtt.h"
#include <TFT_eSPI.h>
#include <SPI.h>

TFT_eSPI tft = TFT_eSPI(135, 240); // Pantalla del TTGO T-Display
#define TFT_BL 4 // Pin de backlight


// Parametros de Ubidots
const char *UBIDOTS_TOKEN  = "TU_TOKEN_AQUI";
const char *WIFI_SSID      = "TU_WIFI_AQUI";
const char *WIFI_PASS      = "TU_PASSWORD_AQUI";
const char *DEVICE_LABEL   = "gilbert_ttgo";
const char *VARIABLE_LABEL = "control"; // Crea esta variable en el dashboard con un Slider o Switch

Ubidots ubidots(UBIDOTS_TOKEN);
unsigned long lastReceived = 0; // marca de tiempo del ultimo dato recibido


// Callback: aqui llega cada vez que "control" cambia de valor en Ubidots
void callback(char *topic, byte *payload, unsigned int length) {
  // El payload del topico /lv es texto plano, no JSON
  String valorTexto = "";
  for (unsigned int i = 0; i < length; i++) {
    valorTexto += (char)payload[i];
  }
  float valor = valorTexto.toFloat();

  Serial.print("Valor recibido de Ubidots: ");
  Serial.println(valor);

  lastReceived = millis();

  // Mostrar el valor recibido en pantalla
  tft.fillRect(10, 60, 220, 30, TFT_BLACK); // borra el valor anterior
  tft.setCursor(10, 60);
  tft.setTextSize(3);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.println(valor, 2);
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
  tft.println("Ubidots -> ESP32");
  tft.setTextSize(1);
  tft.setCursor(10, 40);
  tft.println("Valor recibido:");

  ubidots.setDebug(true); // ver logs de conexion y suscripcion

  ubidots.connectToWifi(WIFI_SSID, WIFI_PASS);
  ubidots.setCallback(callback);
  ubidots.setup();
  ubidots.reconnect();
  ubidots.subscribeLastValue(DEVICE_LABEL, VARIABLE_LABEL);
}

void loop() {
  if (!ubidots.connected()) {
    ubidots.reconnect();
    ubidots.subscribeLastValue(DEVICE_LABEL, VARIABLE_LABEL); // resuscribir tras reconectar
  }

  // Mostrar hace cuantos segundos llego el ultimo dato (para confirmar que es tiempo real)
  tft.fillRect(10, 100, 220, 20, TFT_BLACK);
  tft.setCursor(10, 100);
  tft.setTextSize(1);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.print("Hace ");
  tft.print((millis() - lastReceived) / 1000);
  tft.println("s");

  ubidots.loop();
  delay(200);
}
