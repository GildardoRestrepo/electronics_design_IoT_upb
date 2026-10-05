/*
 * conexion.cpp - Practica #6 (Diseño Electronico)
 *
 * WiFi + MQTT hacia el broker Mosquitto que corre en la instancia EC2.
 */

#include <WiFi.h>
#include "conexion.h"
#include "secrets.h" // WIFI_*, MQTT_BROKER, MQTT_PORT, MQTT_USER, MQTT_PASS

WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);

static String clientId;
static const char *topicEstado = nullptr;
static void (*alConectar)() = nullptr;

static unsigned long ultimoIntentoWiFi = 0;
static const unsigned long INTERVALO_WIFI = 15000;

static unsigned long ultimoIntentoMQTT = 0;
static const unsigned long INTERVALO_MQTT = 5000;

static bool wifiAntes = false;
static bool mqttAntes = false;

// ------------------------------------------------------
// Traduce el codigo de PubSubClient a la causa probable. Con el broker en
// Internet los fallos ya no son "el PC esta apagado": casi siempre son la
// regla del Security Group, la IP publica que cambio o las credenciales.
// ------------------------------------------------------
const char *mqttMotivo()
{
  switch (mqtt.state())
  {
  case MQTT_CONNECTED:
    return "conectado";
  case MQTT_CONNECT_FAILED: // -2: no hubo conexion TCP
    return "sin TCP (IP/puerto/SG)";
  case MQTT_CONNECTION_TIMEOUT: // -4
    return "timeout keepalive";
  case MQTT_CONNECTION_LOST: // -3
    return "conexion perdida";
  case MQTT_CONNECT_BAD_CREDENTIALS: // 4
  case MQTT_CONNECT_UNAUTHORIZED:    // 5
    return "usuario/clave rechazados";
  case MQTT_CONNECT_BAD_CLIENT_ID: // 2
    return "client id rechazado";
  default:
    return "desconectado";
  }
}

static void conectarWiFi()
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
    Serial.print("\nWiFi conectado. IP local: ");
    Serial.println(WiFi.localIP());
  }
  else
  {
    Serial.println("\nNo se pudo conectar al WiFi, se reintentara.");
  }
}

static bool conectarMQTT()
{
  Serial.print("Conectando a ");
  Serial.print(MQTT_BROKER);
  Serial.print(":");
  Serial.print(MQTT_PORT);
  Serial.print(" como '");
  Serial.print(MQTT_USER);
  Serial.print("' ... ");

  // Igual que en la practica 05 se declara un LWT, pero ahora con usuario y
  // contraseña: el broker esta en Internet y no acepta anonimos.
  //
  // AVISO: en el puerto 1883 el usuario y la contraseña viajan SIN cifrar.
  // Es aceptable para la practica; en produccion se usaria TLS (8883).
  bool ok = mqtt.connect(clientId.c_str(),
                         MQTT_USER,
                         MQTT_PASS,
                         topicEstado, // will topic
                         1,           // will QoS
                         true,        // will retained
                         "offline");  // will message

  if (ok)
  {
    Serial.println("conectado.");
    // "online" retenido: quien se suscriba despues sabe al instante que el
    // dispositivo esta vivo. El LWT lo sobrescribira con "offline" si cae.
    mqtt.publish(topicEstado, "online", true);
    if (alConectar)
    {
      alConectar();
    }
  }
  else
  {
    Serial.print("fallo, rc=");
    Serial.print(mqtt.state());
    Serial.print(" (");
    Serial.print(mqttMotivo());
    Serial.println(")");
  }
  return ok;
}

void conexionIniciar(const char *prefijoId,
                     const char *topicEstadoLWT,
                     void (*funcionAlConectar)(),
                     MQTT_CALLBACK_SIGNATURE)
{
  topicEstado = topicEstadoLWT;
  alConectar = funcionAlConectar;

  // Client id unico a partir de la MAC. Con dos placas en el mismo broker es
  // obligatorio: si comparten id, el broker expulsa a una cada vez que la
  // otra se conecta y quedan reconectandose en bucle.
  clientId = String(prefijoId) + "-" + String((uint32_t)ESP.getEfuseMac(), HEX);
  Serial.print("\nClient ID MQTT: ");
  Serial.println(clientId);

  mqtt.setServer(MQTT_BROKER, MQTT_PORT);
  mqtt.setCallback(callback);
  mqtt.setKeepAlive(15); // valor por defecto, explicito: fija el retardo del LWT (~1,5x)

  conectarWiFi();
  if (WiFi.status() == WL_CONNECTED)
  {
    conectarMQTT();
  }
  wifiAntes = (WiFi.status() == WL_CONNECTED);
  mqttAntes = mqtt.connected();
}

bool conexionMantener()
{
  unsigned long ahora = millis();

  if (WiFi.status() != WL_CONNECTED)
  {
    if (ahora - ultimoIntentoWiFi >= INTERVALO_WIFI)
    {
      ultimoIntentoWiFi = ahora;
      conectarWiFi();
    }
  }
  else if (!mqtt.connected())
  {
    if (ahora - ultimoIntentoMQTT >= INTERVALO_MQTT)
    {
      ultimoIntentoMQTT = ahora;
      conectarMQTT();
    }
  }

  mqtt.loop(); // procesa mensajes entrantes y envia el PINGREQ del keepalive

  bool wifiAhora = (WiFi.status() == WL_CONNECTED);
  bool mqttAhora = mqtt.connected();
  bool cambio = (wifiAhora != wifiAntes) || (mqttAhora != mqttAntes);
  wifiAntes = wifiAhora;
  mqttAntes = mqttAhora;
  return cambio;
}

bool wifiConectado()
{
  return WiFi.status() == WL_CONNECTED;
}

String ipLocal()
{
  return wifiConectado() ? WiFi.localIP().toString() : String("sin WiFi");
}
