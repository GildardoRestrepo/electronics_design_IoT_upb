# Actividades de Diseño Electrónico — IoT y Sistemas Embebidos

Repositorio de las actividades entregables de la asignatura **Diseño Electrónico**, enfocadas
en **IoT y sistemas embebidos**. El hardware base es una placa **ESP32 TTGO T-Display**
(pantalla TFT ST7789V de 135×240) con un sensor **DHT11**, publicando y recibiendo datos
desde la plataforma **Ubidots** por **MQTT**.

**Autor:** Gildardo Estevan Restrepo Duque

---

## Hardware utilizado

| Componente        | Detalle                                              |
| ----------------- | ---------------------------------------------------- |
| Placa             | ESP32 **TTGO T-Display** (pantalla ST7789V 135×240)  |
| Sensor            | **DHT11** (temperatura y humedad), pin de datos GPIO 25 |
| Backlight TFT     | GPIO 4                                                |
| Conectividad      | Wi-Fi + MQTT hacia **Ubidots**                        |

## Software y librerías

- **Arduino IDE** con soporte para placas ESP32.
- [`UbidotsEsp32Mqtt`](https://github.com/ubidots/ubidots-esp32-mqtt) — conexión MQTT a Ubidots.
- [`TFT_eSPI`](https://github.com/Bodmer/TFT_eSPI) — control de la pantalla.
- `DHT sensor library` (Adafruit) — lectura del DHT11.

> **Configuración de TFT_eSPI (una sola vez):** en `User_Setup_Select.h` de la librería,
> comenta `#include <User_Setup.h>` y descomenta
> `#include <User_Setups/Setup25_TTGO_T_Display.h>`.

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
├── _docs/                          # Bitácora y documentación de cada práctica
│   ├── 01_ubidots_connection.md
│   ├── 02_ubidots_subscription.md
│   └── 03_webserver_accesspoint.md
├── LICENSE
└── README.md
```

Cada carpeta `*.ino` es un sketch independiente de Arduino (el nombre del `.ino` coincide con
el de su carpeta contenedora, como exige el Arduino IDE).

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

Consultar la bitácora de cada práctica en [`_docs/`](_docs):
[Práctica 1](_docs/01_ubidots_connection.md) ·
[Práctica 2](_docs/02_ubidots_subscription.md) ·
[Práctica 3](_docs/03_webserver_accesspoint.md).

---

## Cómo compilar y cargar

1. Instala el soporte de placas **ESP32** en el Arduino IDE y las librerías listadas arriba.
2. Aplica la configuración de `TFT_eSPI` para el TTGO T-Display (ver nota arriba).
3. Abre la carpeta del sketch que quieras probar (p. ej. `01_ubidots_connection/ubidots_dht11`).
4. Completa tus credenciales de Wi-Fi y tu token de Ubidots (ver **Configuración de credenciales**).
5. Selecciona la placa **TTGO T-Display / ESP32 Dev Module**, el puerto correcto y carga.

---

## Configuración de credenciales

Los sketches necesitan red Wi-Fi y tu token de Ubidots. Reemplaza los marcadores por
valores **locales**:

```cpp
const char *UBIDOTS_TOKEN = "TU_TOKEN_AQUI";
const char *WIFI_SSID     = "TU_WIFI_AQUI";
const char *WIFI_PASS     = "TU_PASSWORD_AQUI";
```

---

## Licencia

Distribuido bajo la licencia **MIT**. Consulta el archivo [LICENSE](LICENSE).
