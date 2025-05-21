#include "webserver.h"
#include "utils.h"
#include <FS.h>
#include <SPIFFS.h>
#include <vector>     // Necesario para ordenar (el JS lo hará, pero buena práctica si el ESP32 pre-procesa)
#include <algorithm>  // Necesario para std::sort

const char* ssid = "sochoag";       // ¡Reemplaza con tu SSID de WiFi!
const char* password = "sochoagu"; // ¡Reemplaza con tu contraseña de WiFi!

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

std::map<String, Tarjeta>* globalTarjetasActivasPtr;

void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      Serial.printf("Cliente WebSocket conectado #%u desde %s\n", client->id(), client->remoteIP().toString().c_str());
      sendWebSocketData(); // Enviar el estado actual al nuevo cliente
      break;
    case WS_EVT_DISCONNECT:
      Serial.printf("Cliente WebSocket desconectado #%u\n", client->id());
      break;
    case WS_EVT_DATA:
      Serial.printf("Mensaje recibido de cliente #%u: %s\n", client->id(), (char*)data);
      break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
  }
}

void initWebServer(std::map<String, Tarjeta>& tarjetas) {
  globalTarjetasActivasPtr = &tarjetas;

  if (!SPIFFS.begin(true)) {
    Serial.println("Error al montar SPIFFS");
    return;
  }
  Serial.println("SPIFFS montado exitosamente");

  Serial.print("Conectando a WiFi: ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  int retries = 0;
  while (WiFi.status() != WL_CONNECTED && retries < 40) {
    delay(500);
    Serial.print(".");
    retries++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi conectado.");
    Serial.print("Dirección IP: ");
    Serial.println(WiFi.localIP());

    ws.onEvent(onWsEvent);
    server.addHandler(&ws);

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
      request->send(SPIFFS, "/index.html", "text/html");
    });

    server.onNotFound([](AsyncWebServerRequest *request){
      request->send(404, "text/plain", "Not Found");
    });

    server.begin();
    Serial.println("Servidor web iniciado");
  } else {
    Serial.println("\nFallo al conectar a WiFi. Por favor, revisa tu SSID y contraseña.");
  }
}

// Función para enviar los datos de las tarjetas a todos los clientes WebSocket conectados
void sendWebSocketData() {
  String jsonString = "{"; // Ahora enviamos un objeto JSON principal
  jsonString += "\"timestampEsp32\": " + String(millis()) + ","; // Añadimos el timestamp actual del ESP32
  jsonString += "\"tarjetas\": ["; // El array de tarjetas estará anidado

  if (globalTarjetasActivasPtr->empty()) {
    jsonString += "]"; // Cierra el array si está vacío
  } else {
    // Creamos un vector de punteros a Tarjeta para poder ordenarlos
    std::vector<const Tarjeta*> sortedTarjetas;
    for (auto const& pair : *globalTarjetasActivasPtr) {
      sortedTarjetas.push_back(&pair.second);
    }

    // Ordenar el vector por ID de grupo
    std::sort(sortedTarjetas.begin(), sortedTarjetas.end(), [](const Tarjeta* a, const Tarjeta* b) {
      return a->getGroupId() < b->getGroupId();
    });

    bool firstEntry = true;
    for (const Tarjeta* tarjetaPtr : sortedTarjetas) {
      const Tarjeta& tarjeta = *tarjetaPtr;

      if (!firstEntry) {
        jsonString += ",";
      }
      firstEntry = false;

      jsonString += "{";
      jsonString += "\"uid\": \"" + tarjeta.getUid() + "\",";
      jsonString += "\"grupo\": \"Grupo " + String(tarjeta.getGroupId()) + "\",";
      
      if (tarjeta.isActive()) {
          jsonString += "\"startTime\": " + String(tarjeta.getStartTime()) + ","; // Enviamos el startTime del ESP32
          jsonString += "\"isActive\": true";
      } else {
          jsonString += "\"tiempoTotalFormatted\": \"" + tarjeta.getTotalTime() + "\","; // Tiempo ya formateado para inactivas
          jsonString += "\"isActive\": false";
      }
      jsonString += "}";
    }
    jsonString += "]"; // Cierra el array de tarjetas
  }

  jsonString += "}"; // Cierra el objeto JSON principal
  ws.textAll(jsonString);
  Serial.println("Datos WebSocket enviados (para cálculo en cliente).");
  // Serial.println(jsonString); // Descomentar para ver el JSON completo en Serial
}