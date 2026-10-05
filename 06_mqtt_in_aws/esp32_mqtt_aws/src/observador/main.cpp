/*
 * Practica #6 - Diseño Electronico (IoT / Sistemas Embebidos)
 * Firmware "observador" (cliente 2 de la arquitectura).
 *
 * Segundo TTGO T-Display SIN sensor. No publica datos: solo se suscribe a lo
 * que publica el sensor y lo muestra. Puede estar en otra red, otra ciudad u
 * otro pais: lo unico que comparte con el sensor es el broker en AWS.
 *
 *   Se suscribe: temperatura, sw y estado (LWT) del sensor
 *   Publica    : solo su propio estado (online / LWT offline)
 */

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <SPI.h>

#include "secrets.h"
#include "topicos.h"
#include "conexion.h"

TFT_eSPI tft = TFT_eSPI(135, 240);

// ======================================================
// ESTADO (todo llega por MQTT)
// ======================================================
String temperatura = "--";
bool swPulsado = false;
String estadoSensor = "?"; // "online" / "offline" (LWT) / "?" sin datos

bool refrescarPantalla = true;

// ======================================================
// PANTALLA
// ======================================================
void mostrarDatos()
{
  tft.fillScreen(TFT_BLACK);

  // ---- Temperatura remota, en grande ----
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(1);
  tft.setCursor(8, 6);
  tft.print("Temperatura del sensor remoto");
  tft.setTextSize(4);
  tft.setCursor(8, 22);
  tft.print(temperatura);
  tft.setTextSize(2);
  tft.print(" C");

  // ---- sw remoto ----
  tft.fillCircle(212, 38, 16, swPulsado ? TFT_YELLOW : TFT_DARKGREY);
  tft.setTextSize(1);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setCursor(205, 60);
  tft.print("SW");

  // ---- Estado del sensor (alimentado por su LWT) ----
  tft.setCursor(8, 74);
  tft.print("Sensor: ");
  if (estadoSensor == "online")
  {
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.print("CONECTADO");
  }
  else if (estadoSensor == "offline")
  {
    // Lo publico el BROKER, no el sensor: el sensor ya no puede hablar.
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.print("DESCONECTADO (LWT)");
  }
  else
  {
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.print("sin datos");
  }

  // ---- Conexion propia ----
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setCursor(8, 94);
  tft.print("MQTT: ");
  tft.setTextColor(mqtt.connected() ? TFT_GREEN : TFT_RED, TFT_BLACK);
  tft.print(mqttMotivo());

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setCursor(8, 108);
  tft.print("Broker: ");
  tft.print(MQTT_BROKER);

  tft.setCursor(8, 122);
  tft.print("IP local: ");
  tft.print(ipLocal());
}

// ======================================================
// MQTT
// ======================================================
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

  if (t == TOPIC_TEMP)
  {
    temperatura = mensaje;
  }
  else if (t == TOPIC_SW)
  {
    swPulsado = (mensaje == "1");
  }
  else if (t == TOPIC_ESTADO)
  {
    estadoSensor = mensaje;
  }
  refrescarPantalla = true;
}

void alConectar()
{
  // Como el sensor publica todo RETENIDO, estas tres suscripciones devuelven
  // de inmediato el ultimo valor conocido: la pantalla no arranca vacia.
  mqtt.subscribe(TOPIC_TEMP);
  mqtt.subscribe(TOPIC_SW);
  mqtt.subscribe(TOPIC_ESTADO);
  Serial.println("Suscrito a temperatura, sw y estado del sensor");
  refrescarPantalla = true;
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
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(10, 40);
  tft.print("Practica 06");
  tft.setTextSize(1);
  tft.setCursor(10, 70);
  tft.print("MQTT en AWS - observador");

  conexionIniciar("esp32-observador", TOPIC_ESTADO_OBS, alConectar, callback);
  mostrarDatos();
}

void loop()
{
  if (conexionMantener())
  {
    refrescarPantalla = true;
  }

  if (refrescarPantalla)
  {
    refrescarPantalla = false;
    mostrarDatos();
  }
}
