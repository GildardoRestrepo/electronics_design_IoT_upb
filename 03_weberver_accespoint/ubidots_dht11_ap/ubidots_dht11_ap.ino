/*
 * Practica #3 - Diseño Electronico (IoT / Sistemas Embebidos)
 * Monitoreo de temperatura y humedad con ESP32 (TTGO T-Display) + DHT11 + Ubidots,
 * con modo Access Point y servidor web para reconfigurar el WiFi.
 *
 * Hibrido: mantiene la config fisica y el device label de las practicas 01/02,
 * pero usa MQTT crudo con PubSubClient (en vez de la libreria UbidotsEsp32Mqtt).
 */

#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <WebServer.h>
#include <Preferences.h>

// ======================================================
// CONFIGURACION FISICA (igual que practicas 01 y 02)
// ======================================================
TFT_eSPI tft = TFT_eSPI(135, 240); // Pantalla del TTGO T-Display
#define TFT_BL 4  // Pin de backlight

#define DHTPIN 25 // Pin de datos del DHT11
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// ======================================================
// WIFI PREDETERMINADO (placeholders: pon tus datos localmente)
// ======================================================
const char *DEFAULT_SSID     = "TU_WIFI_AQUI";
const char *DEFAULT_PASSWORD = "TU_PASSWORD_AQUI";

// ======================================================
// ACCESS POINT DEL ESP32 (red propia para reconfigurar el WiFi)
// ======================================================
const char *AP_SSID     = "ESP32-gilbert";
const char *AP_PASSWORD  = "12345678"; // minimo 8 caracteres que exige el ESP32

// ======================================================
// CONFIGURACION UBIDOTS (MQTT crudo)
// ======================================================
const char *UBIDOTS_TOKEN = "TU_TOKEN_AQUI";
const char *MQTT_SERVER   = "industrial.api.ubidots.com";
const int   MQTT_PORT     = 1883;

const char *DEVICE_LABEL  = "gilbert_ttgo"; // mismo dispositivo de las practicas anteriores
const char *VAR_TEMP      = "temperatura";
const char *VAR_HUM       = "humedad";
const char *VAR_SW1       = "sw1"; // switch 1 en el dashboard de Ubidots
const char *VAR_SW2       = "sw2"; // switch 2 en el dashboard de Ubidots

// ======================================================
// OBJETOS GLOBALES
// ======================================================
WiFiClient espClient;
PubSubClient client(espClient);
WebServer server(80);
Preferences preferences;

// Credenciales WiFi activas (se cargan desde memoria en el arranque)
String wifiSSID;
String wifiPassword;

// ======================================================
// VARIABLES DE ESTADO
// ======================================================
int sw1 = 0; // estado recibido de Ubidots
int sw2 = 0;

float temperatura = 0.0; // ultimos valores medidos
float humedad     = 0.0;

// ======================================================
// CONTROL DE TIEMPOS (loop no bloqueante con millis())
// ======================================================
unsigned long tiempoAnterior = 0;
const unsigned long PUBLISH_FREQUENCY = 5000; // el DHT11 solo acepta 1 lectura/segundo

unsigned long ultimoIntentoWiFi = 0;
const unsigned long INTERVALO_WIFI = 15000; // reintento de reconexion WiFi

unsigned long ultimoIntentoMQTT = 0;
const unsigned long INTERVALO_MQTT = 5000;  // reintento de reconexion MQTT

// ======================================================
// MOSTRAR DATOS EN LA PANTALLA (layout adaptado a 240x135)
// ======================================================
void mostrarDatos(float temp, float hum) {
  tft.fillScreen(TFT_BLACK);

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);

  tft.setCursor(10, 10);
  tft.print("Temp: ");
  tft.print(temp, 1);
  tft.println(" C");

  tft.setCursor(10, 40);
  tft.print("Hum:  ");
  tft.print(hum, 1);
  tft.println(" %");

  // Color del circulo segun los switches recibidos de Ubidots
  uint16_t color;
  if (sw1 == 1 && sw2 == 1) {
    color = TFT_GREEN;
  } else if (sw1 == 1) {
    color = TFT_RED;
  } else if (sw2 == 1) {
    color = TFT_BLUE;
  } else {
    color = TFT_DARKGREY;
  }
  tft.fillCircle(205, 40, 18, color);

  // Estado de los switches
  tft.setTextSize(1);
  tft.setCursor(10, 95);
  tft.print("SW1: ");
  tft.print(sw1);
  tft.setCursor(10, 110);
  tft.print("SW2: ");
  tft.print(sw2);

  // IP local (util para saber si esta conectado al router)
  tft.setCursor(90, 110);
  tft.print("IP: ");
  tft.print(WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : "AP");
}

// ======================================================
// MOSTRAR ERROR DEL DHT11
// ======================================================
void mostrarErrorDHT11() {
  tft.fillScreen(TFT_BLACK);

  tft.setTextColor(TFT_RED, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(10, 20);
  tft.println("Error DHT11");

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(1);
  tft.setCursor(10, 55);
  tft.println("Revise el sensor");
  tft.setCursor(10, 70);
  tft.println("Senal: GPIO 25");
}

// ======================================================
// CALLBACK MQTT: llega cuando cambia sw1 o sw2 en Ubidots
// ======================================================
void callback(char *topic, byte *payload, unsigned int length) {
  // El payload del topico /lv es texto plano, no JSON
  String message = "";
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  String topicRecibido = String(topic);
  int value = message.toInt();

  Serial.print("\nMQTT recibido [");
  Serial.print(topicRecibido);
  Serial.print("] valor: ");
  Serial.println(value);

  if (topicRecibido.endsWith("/sw1/lv")) {
    sw1 = value;
    Serial.println("SW1 actualizado");
  } else if (topicRecibido.endsWith("/sw2/lv")) {
    sw2 = value;
    Serial.println("SW2 actualizado");
  }

  mostrarDatos(temperatura, humedad); // refrescar color del circulo al instante
}

// ======================================================
// CARGAR CONFIGURACION WIFI desde memoria (Preferences / NVS)
// ======================================================
void cargarWiFi() {
  preferences.begin("wifi", true); // true = solo lectura
  wifiSSID     = preferences.getString("ssid", DEFAULT_SSID);
  wifiPassword = preferences.getString("password", DEFAULT_PASSWORD);
  preferences.end();

  Serial.println("\nWiFi cargado de memoria:");
  Serial.print("SSID: ");
  Serial.println(wifiSSID);
}

// ======================================================
// GUARDAR NUEVA CONFIGURACION WIFI en memoria
// ======================================================
void guardarWiFi(String nuevoSSID, String nuevaPassword) {
  preferences.begin("wifi", false); // false = lectura/escritura
  preferences.putString("ssid", nuevoSSID);
  preferences.putString("password", nuevaPassword);
  preferences.end();
  Serial.println("Nueva configuracion WiFi guardada");
}

// ======================================================
// RESTAURAR WIFI PREDETERMINADO (borra lo guardado en NVS)
// ======================================================
void restaurarWiFi() {
  preferences.begin("wifi", false);
  preferences.clear();
  preferences.end();
  Serial.println("WiFi restaurado a valores predeterminados");
}

// ======================================================
// INICIAR ACCESS POINT (el ESP32 crea su propia red)
// ======================================================
void iniciarAccessPoint() {
  WiFi.mode(WIFI_AP_STA); // cliente del router Y punto de acceso a la vez

  if (WiFi.softAP(AP_SSID, AP_PASSWORD)) {
    Serial.println("\n==============================");
    Serial.println("ACCESS POINT INICIADO");
    Serial.print("Nombre:   "); Serial.println(AP_SSID);
    Serial.print("Password: "); Serial.println(AP_PASSWORD);
    Serial.print("IP:       "); Serial.println(WiFi.softAPIP());
    Serial.println("==============================");
  } else {
    Serial.println("Error creando Access Point");
  }
}

// ======================================================
// CONECTARSE AL WIFI DEL ROUTER
// ======================================================
void conectarWiFi() {
  Serial.print("\nConectando a: ");
  Serial.println(wifiSSID);
  WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());
  ultimoIntentoWiFi = millis();
}

// ======================================================
// PAGINA WEB DE CONFIGURACION (servida en 192.168.4.1)
// ======================================================
String crearPaginaWeb() {
  String pagina = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>ESP32 DHT11</title>
<style>
body { font-family: Arial; background: #eeeeee; text-align: center; margin: 0; padding: 20px; }
.contenedor { background: white; max-width: 450px; margin: auto; padding: 25px; border-radius: 15px; box-shadow: 0px 2px 10px rgba(0,0,0,0.2); }
h1 { color: #333333; }
h2 { color: #555555; }
input { width: 90%; padding: 12px; margin: 8px; border: 1px solid #cccccc; border-radius: 8px; }
button { width: 95%; padding: 12px; margin-top: 10px; border: none; border-radius: 8px; background: #2196F3; color: white; font-size: 16px; }
button:hover { background: #1976D2; }
.botonRojo { background: #f44336; }
.estado { background: #eeeeee; padding: 10px; border-radius: 10px; margin-bottom: 20px; }
</style>
</head>
<body>
<div class="contenedor">
<h1>ESP32 DHT11</h1>
<div class="estado">
<h2>Estado WiFi</h2>
)rawliteral";

  if (WiFi.status() == WL_CONNECTED) {
    pagina += "<p><b>Conectado a:</b> " + WiFi.SSID() + "</p>";
    pagina += "<p><b>IP local:</b> " + WiFi.localIP().toString() + "</p>";
  } else {
    pagina += "<p><b>No conectado al router</b></p>";
    pagina += "<p>Intentando conectar a: " + wifiSSID + "</p>";
  }

  pagina += R"rawliteral(
</div>
<h2>Configurar WiFi</h2>
<form action="/guardar" method="POST">
<input type="text" name="ssid" placeholder="Nombre de la red WiFi" required>
<br>
<input type="password" name="password" placeholder="Contrasena">
<br>
<button type="submit">Guardar WiFi</button>
</form>
<br>
<form action="/restaurar" method="POST">
<button class="botonRojo" type="submit">Restaurar WiFi predeterminado</button>
</form>
</div>
</body>
</html>
)rawliteral";

  return pagina;
}

// ======================================================
// CONFIGURAR RUTAS DEL SERVIDOR WEB
// ======================================================
void configurarServidorWeb() {
  // Pagina principal
  server.on("/", HTTP_GET, []() {
    server.send(200, "text/html", crearPaginaWeb());
  });

  // Guardar nueva red y reiniciar
  server.on("/guardar", HTTP_POST, []() {
    String nuevoSSID     = server.arg("ssid");
    String nuevaPassword = server.arg("password");

    if (nuevoSSID.length() == 0) {
      server.send(400, "text/plain", "SSID invalido");
      return;
    }

    guardarWiFi(nuevoSSID, nuevaPassword);
    server.send(200, "text/html",
      "<meta charset='UTF-8'><body style='font-family:Arial;text-align:center;padding:30px;'>"
      "<h2>WiFi guardado correctamente</h2><p>El ESP32 se reiniciara.</p></body>");
    delay(1500);
    ESP.restart();
  });

  // Restaurar red predeterminada y reiniciar
  server.on("/restaurar", HTTP_POST, []() {
    restaurarWiFi();
    server.send(200, "text/html",
      "<meta charset='UTF-8'><body style='font-family:Arial;text-align:center;padding:30px;'>"
      "<h2>WiFi restaurado</h2><p>Se usara nuevamente la red predeterminada.</p>"
      "<p>El ESP32 se reiniciara.</p></body>");
    delay(1500);
    ESP.restart();
  });

  server.begin();
  Serial.println("\nServidor web iniciado en http://192.168.4.1");
}

// ======================================================
// CONEXION / RECONEXION MQTT (Ubidots)
// ======================================================
void reconnectMQTT() {
  if (client.connected() || WiFi.status() != WL_CONNECTED) {
    return;
  }

  Serial.print("Conectando a Ubidots MQTT...");

  // ID unico de cliente basado en la MAC del ESP32
  String clientId = "ESP32-" + String((uint32_t)ESP.getEfuseMac(), HEX);

  // En Ubidots el usuario MQTT es el token y la clave va vacia
  if (client.connect(clientId.c_str(), UBIDOTS_TOKEN, "")) {
    Serial.println(" conectado");

    // Suscribirse al ultimo valor (/lv) de sw1 y sw2
    String topicSw1 = String("/v1.6/devices/") + DEVICE_LABEL + "/" + VAR_SW1 + "/lv";
    String topicSw2 = String("/v1.6/devices/") + DEVICE_LABEL + "/" + VAR_SW2 + "/lv";
    client.subscribe(topicSw1.c_str());
    client.subscribe(topicSw2.c_str());

    Serial.print("Suscrito a: "); Serial.println(topicSw1);
    Serial.print("Suscrito a: "); Serial.println(topicSw2);
  } else {
    Serial.print("Error MQTT, codigo: ");
    Serial.println(client.state());
  }
}

// ======================================================
// PUBLICAR TEMPERATURA Y HUMEDAD EN UBIDOTS
// ======================================================
void publicarDatos(float temp, float hum) {
  if (!client.connected()) {
    return;
  }

  String topic = String("/v1.6/devices/") + DEVICE_LABEL;

  // Payload JSON: {"temperatura":25.0,"humedad":50.0}
  String payload = "{\"";
  payload += VAR_TEMP; payload += "\":"; payload += String(temp, 1);
  payload += ",\"";
  payload += VAR_HUM;  payload += "\":"; payload += String(hum, 1);
  payload += "}";

  bool enviado = client.publish(topic.c_str(), payload.c_str());

  Serial.print("Publicado en ");
  Serial.print(topic);
  Serial.print(" -> ");
  Serial.print(payload);
  Serial.println(enviado ? "  [OK]" : "  [FALLO]");
}

// ======================================================
// SETUP
// ======================================================
void setup() {
  Serial.begin(115200);
  delay(500);

  dht.begin();

  // Pantalla + backlight (config del TTGO T-Display)
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(10, 10);
  tft.println("Iniciando...");

  cargarWiFi();          // 1. leer credenciales guardadas
  iniciarAccessPoint();  // 2. crear la red ESP32-gilbert
  conectarWiFi();        // 3. intentar conexion al router
  configurarServidorWeb(); // 4. servidor de configuracion

  // 5. MQTT
  client.setServer(MQTT_SERVER, MQTT_PORT);
  client.setCallback(callback);
  client.setBufferSize(512); // margen para topicos/payloads largos
}

// ======================================================
// LOOP (no bloqueante)
// ======================================================
void loop() {
  server.handleClient(); // atender el portal de configuracion

  // --- Control del WiFi ---
  if (WiFi.status() != WL_CONNECTED) {
    if (millis() - ultimoIntentoWiFi >= INTERVALO_WIFI) {
      Serial.println("\nWiFi desconectado. Reintentando...");
      WiFi.disconnect();
      delay(100);
      conectarWiFi();
    }
  } else {
    // --- MQTT solo si hay WiFi ---
    if (!client.connected()) {
      if (millis() - ultimoIntentoMQTT >= INTERVALO_MQTT) {
        ultimoIntentoMQTT = millis();
        reconnectMQTT();
      }
    } else {
      client.loop(); // mantiene viva la conexion y procesa suscripciones
    }
  }

  // --- Lectura y publicacion periodica del DHT11 ---
  if (millis() - tiempoAnterior >= PUBLISH_FREQUENCY) {
    tiempoAnterior = millis();

    humedad     = dht.readHumidity();
    temperatura = dht.readTemperature(); // Celsius

    // El DHT11 a veces devuelve NaN cuando falla la lectura
    if (isnan(temperatura) || isnan(humedad)) {
      Serial.println("\nError al leer el sensor DHT11");
      mostrarErrorDHT11();
      return;
    }

    Serial.print("\nLectura DHT11 -> Temp: ");
    Serial.print(temperatura, 1);
    Serial.print(" C, Humedad: ");
    Serial.print(humedad, 1);
    Serial.println(" %");

    publicarDatos(temperatura, humedad);
    mostrarDatos(temperatura, humedad);
  }
}
