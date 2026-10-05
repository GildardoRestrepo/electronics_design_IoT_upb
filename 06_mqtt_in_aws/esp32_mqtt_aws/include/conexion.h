/*
 * conexion.h - Practica #6 (Diseño Electronico)
 *
 * WiFi + MQTT compartido por los dos firmwares. Lo que en la practica 05
 * vivia dentro de main.cpp se separa aqui porque ahora hay dos programas
 * (sensor y observador) que se conectan exactamente igual al broker en AWS.
 */

#ifndef CONEXION_H
#define CONEXION_H

#include <Arduino.h>
#include <PubSubClient.h>

// Cliente MQTT unico, accesible desde cada main.cpp para publicar/suscribir.
extern PubSubClient mqtt;

// Prepara WiFi y MQTT y hace el primer intento de conexion.
//   prefijoId   -> el client id sera "<prefijoId>-<MAC>" (unico por placa)
//   topicEstado -> topico del LWT: el broker publicara ahi "offline" si el
//                  dispositivo desaparece sin despedirse
//   alConectar  -> se ejecuta tras cada (re)conexion: suscripciones, etc.
//   callback    -> recibe los mensajes de los topicos suscritos
void conexionIniciar(const char *prefijoId,
                     const char *topicEstado,
                     void (*alConectar)(),
                     MQTT_CALLBACK_SIGNATURE);

// Llamar en cada vuelta del loop(). Reintenta WiFi/MQTT sin bloquear el
// programa y atiende los mensajes entrantes. Devuelve true si cambio el
// estado de la conexion (util para redibujar la pantalla).
bool conexionMantener();

bool wifiConectado();
String ipLocal();

// Texto corto con la causa del ultimo fallo MQTT (para pantalla y serie).
const char *mqttMotivo();

#endif // CONEXION_H
