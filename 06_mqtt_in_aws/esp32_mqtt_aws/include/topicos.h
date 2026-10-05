/*
 * topicos.h - Practica #6 (Diseño Electronico)
 *
 * Esquema de topicos compartido por los dos firmwares (sensor y observador)
 * y por el flujo de Node-RED. Se conserva la jerarquia de la practica 05,
 * <usuario>/<dispositivo>/<señal>, y se adapta a la arquitectura de la guia:
 *
 *   Guia (MAX30100)   Este repositorio (DHT11)       Sentido
 *   ---------------   ---------------------------    ------------------------
 *   esp32/spo2        gilbert/ttgo/temperatura       sensor -> todos
 *   esp32/ritmo       gilbert/ttgo/humedad           sensor -> todos
 *   esp32/sw          gilbert/ttgo/sw                sensor -> todos
 *   esp32/led1        gilbert/ttgo/led1              dashboard -> sensor
 *   esp32/led2        gilbert/ttgo/led2              dashboard -> sensor
 *   (no aparece)      gilbert/ttgo/estado            LWT del sensor
 *   (no aparece)      gilbert/observador/estado      LWT del observador
 */

#ifndef TOPICOS_H
#define TOPICOS_H

// ---- Publicados por el sensor (todos retenidos) ----
#define TOPIC_TEMP    "gilbert/ttgo/temperatura"
#define TOPIC_HUM     "gilbert/ttgo/humedad"
#define TOPIC_SW      "gilbert/ttgo/sw"
#define TOPIC_ESTADO  "gilbert/ttgo/estado"

// ---- Ordenes del dashboard al sensor (retenidas por Node-RED) ----
#define TOPIC_LED1    "gilbert/ttgo/led1"
#define TOPIC_LED2    "gilbert/ttgo/led2"

// ---- Estado (LWT) del segundo cliente ----
#define TOPIC_ESTADO_OBS "gilbert/observador/estado"

#endif // TOPICOS_H
