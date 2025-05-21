#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <Arduino.h>         // <--- Añadir esta línea primero
#include <WiFi.h>
#include <HTTP_Method.h>     // <--- Añadir esta línea (de nuevo), pero AHORA el orden importa.
#include <ESPAsyncWebServer.h> // Ahora se incluye DESPUÉS de las definiciones básicas
#include <map>
#include <string>
#include "Tarjeta.h"

// Declaración de las instancias del servidor web y WebSocket como externas
extern AsyncWebServer server;
extern AsyncWebSocket ws;

// Puntero para acceder al mapa de tarjetas (se pasará desde main.cpp)
extern std::map<String, Tarjeta>* globalTarjetasActivasPtr;

// Declaración de las funciones del servidor web/WebSocket
void initWebServer(std::map<String, Tarjeta>& tarjetas);
void sendWebSocketData();

#endif