# Actividades de Diseño Electrónico — IoT y Sistemas Embebidos

Repositorio de las actividades entregables de la asignatura **Diseño Electrónico**, enfocadas
en **IoT y sistemas embebidos**. El hardware base es una placa **ESP32 TTGO T-Display**
(pantalla TFT ST7789V de 135×240).
**Autor:** Gildardo Estevan Restrepo Duque

---

## Hardware utilizado

| Componente    | Detalle                                                 |
| ------------- | ------------------------------------------------------- |
| Placa         | ESP32 **TTGO T-Display** (pantalla ST7789V 135×240)     |
| Sensor        | **DHT11** (temperatura y humedad), pin de datos GPIO 25 |
| Backlight TFT | GPIO 4                                                  |
| Conectividad  | Wi-Fi + MQTT                                            |

---
## Software y librerías

**Prácticas 01–03 — Arduino IDE:**

- **Arduino IDE** con soporte para placas ESP32.
- [`UbidotsEsp32Mqtt`](https://github.com/ubidots/ubidots-esp32-mqtt) — conexión MQTT a Ubidots.
- [`TFT_eSPI`](https://github.com/Bodmer/TFT_eSPI) — control de la pantalla.
- `DHT sensor library` (Adafruit) — lectura del DHT11.
- `PubSubClient` (Nick O'Leary) — cliente MQTT crudo, desde la práctica 03.

> **Configuración de TFT_eSPI (una sola vez):** en `User_Setup_Select.h` de la librería,
> comenta `#include <User_Setup.h>` y descomenta
> `#include <User_Setups/Setup25_TTGO_T_Display.h>`.

**Práctica 05 — VS Code + PlatformIO y stack MQTT local:**

- **VS Code** con la extensión **PlatformIO**. Las librerías se declaran en `platformio.ini`
  y se descargan solas, con la versión fijada.
- **Eclipse Mosquitto** — broker MQTT propio, en la red local.
- **Node-RED** + paleta `node-red-dashboard` — el dashboard.
- **MQTT Explorer** — inspección del árbol de tópicos.

> En la práctica 05 **no hay que tocar `User_Setup_Select.h`**: la configuración del
> TTGO T-Display va en los `build_flags` de `platformio.ini` y está versionada, así que el
> proyecto compila recién clonado en cualquier máquina.

---

## Estructura del repositorio

```
05_actividades_diseño_electronico/
├── 01_ubidots_connection/          # Práctica 1: conexión y publicación a Ubidots
│   ├── test_pantalla/              #   Prueba básica de la pantalla TFT
│   ├── connection_ubidots_wavetest/#   Publica una onda senoidal de prueba a Ubidots
│   └── ubidots_dht11/              #   Publica lecturas del DHT11 + muestra en pantalla
├── 02_ubidots_subscription/        # Práctica 2: suscripción (downlinks) desde Ubidots
│   ├── ubidots_sub/                #   Suscripción mínima a una variable "control"
│   └── ubidots_dht11_downlink/     #   Publica DHT11 y recibe "control" desde el dashboard
├── 03_weberver_accespoint/         # Práctica 3: MQTT crudo + Access Point + WebServer
│   └── ubidots_dht11_ap/           #   DHT11+Ubidots (PubSubClient) y portal de config WiFi
├── 05_mqtt_local_nodered/           # Práctica 5: broker MQTT local + dashboard Node-RED
│   └── esp32_mqtt_local/            #   Proyecto PlatformIO (no es un sketch .ino)
├── _docs/                          # Bitácora y documentación de cada práctica
│   ├── 01_ubidots_connection.md
│   ├── 02_ubidots_subscription.md
│   ├── 03_webserver_accesspoint.md
│   ├── 04_repositorio_entregables.md
│   └── 05_mqtt_local_nodered.md
├── LICENSE
└── README.md
```

En las prácticas 01–03, cada carpeta `*.ino` es un sketch independiente de Arduino (el
nombre del `.ino` coincide con el de su carpeta contenedora, como exige el Arduino IDE).

La práctica 05 rompe esa convención a propósito: es un **proyecto PlatformIO**, con el
código en `src/main.cpp` y la configuración del proyecto en `platformio.ini`.

---
## Prácticas

### 01 — Conexión a Ubidots
Preparación del entorno, prueba de la pantalla, conexión MQTT a Ubidots con una onda de prueba
y, finalmente, envío de las lecturas del DHT11 mostrando el último dato en pantalla.

### 02 — Suscripción (downlinks)
Además de publicar el DHT11, el dispositivo se **suscribe** a una variable `control` del
dashboard (un Slider o Switch en Ubidots) y reacciona a sus cambios en tiempo real.

### 03 — Servidor web / Access Point
Enfoque **híbrido**: se conserva la configuración física y el `DEVICE_LABEL` de las
prácticas anteriores, pero la conexión a Ubidots pasa a **MQTT crudo con `PubSubClient`**.
El dispositivo levanta un **Access Point** (`ESP32-gilbert`) con un **servidor web** en
`192.168.4.1` que permite cambiar/guardar/restaurar el Wi-Fi, con las credenciales
persistidas en memoria (`Preferences`). Dos switches (`sw1`/`sw2`) del dashboard controlan
el color de un indicador en pantalla.

### 04 — Repositorio de entregables
Creación de este repositorio: se reúnen las prácticas 01–03, se define la estructura por
carpetas numeradas y las convenciones de nombres, se añaden el `.gitignore` (artefactos de
compilación y credenciales), la licencia MIT y las bitácoras de `_docs/`. No añade hardware
ni firmware: el entregable es la propia infraestructura de trabajo.

### 05 — MQTT local: Mosquitto + Node-RED
Se abandona la nube. El broker pasa a ser un **Mosquitto** propio en la red local y el
dashboard se construye en **Node-RED**, que no es más que otro cliente MQTT. Al desaparecer
la plataforma intermedia hay que resolver a mano tres mecanismos del protocolo: el
**esquema de tópicos**, los **mensajes retenidos** y el **LWT** (el broker anuncia por su
cuenta la caída del dispositivo). Incluye un canal de alertas que demuestra el reparto
**uno a muchos**. El entorno cambia a **VS Code + PlatformIO**.

Consultar la bitácora de cada práctica en [`_docs/`](_docs):
[Práctica 1](_docs/01_ubidots_connection.md) ·
[Práctica 2](_docs/02_ubidots_subscription.md) ·
[Práctica 3](_docs/03_webserver_accesspoint.md) ·
[Práctica 4](_docs/04_repositorio_entregables.md) ·
[Práctica 5](_docs/05_mqtt_local_nodered.md).

---

## Cómo compilar y cargar

### Prácticas 01–03 (Arduino IDE)

1. Instala el soporte de placas **ESP32** en el Arduino IDE y las librerías listadas arriba.
2. Aplica la configuración de `TFT_eSPI` para el TTGO T-Display (ver nota arriba).
3. Abre la carpeta del sketch que quieras probar (p. ej. `01_ubidots_connection/ubidots_dht11`).
4. Completa tus credenciales de Wi-Fi y tu token de Ubidots (ver **Configuración de credenciales**).
5. Selecciona la placa **TTGO T-Display / ESP32 Dev Module**, el puerto correcto y carga.

### Práctica 05 (PlatformIO)

1. Abre la carpeta `05_mqtt_local_nodered/esp32_mqtt_local` en VS Code con PlatformIO.
2. Copia `include/secrets.h.example` como `include/secrets.h` y rellena tu Wi-Fi y la IP
   del PC donde corre Mosquitto.
3. Compila y carga (`PlatformIO: Upload`). Las librerías se descargan solas.

> **Carga en el TTGO T-Display:** el auto-reset por DTR/RTS falla a menudo en esta placa y
> aparece `Failed to connect to ESP32: No serial data received`. En ese caso hay que forzar
> el modo bootloader: desconectar el USB, mantener pulsado **BOOT (GPIO 0)**, reconectar sin
> soltar y soltar tras dos segundos. **Tras grabar hace falta otro ciclo de alimentación**,
> porque el chip se queda en modo descarga y no ejecuta el firmware recién grabado.

---

## Configuración de credenciales

Ningún valor real de Wi-Fi ni de token se versiona. El repositorio usa dos esquemas:

**Prácticas 01–03** — marcadores dentro del propio sketch, que se completan localmente:

```cpp
const char *UBIDOTS_TOKEN = "TU_TOKEN_AQUI";
const char *WIFI_SSID     = "TU_WIFI_AQUI";
const char *WIFI_PASS     = "TU_PASSWORD_AQUI";
```

**Práctica 05** — archivo aparte, ignorado por git. Copia
`esp32_mqtt_local/include/secrets.h.example` como `secrets.h` y rellénalo. Al estar
`secrets.h` en el `.gitignore`, no se puede subir por descuido.

> ⚠️ Un secreto publicado en un repositorio **queda en el historial de git aunque después
> se borre del archivo**. Por eso los valores reales viven solo en tu copia local.

---

## Licencia

Distribuido bajo la licencia **MIT**. Consulta el archivo [LICENSE](LICENSE).

