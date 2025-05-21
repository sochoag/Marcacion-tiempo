#include "webserver.h"
#include "utils.h"
#include <FS.h>
#include <SPIFFS.h>
#include <vector>
#include <algorithm>
#include <ArduinoJson.h> // Necesario para parsear JSON

const char* ssid = "sochoag";       // ¡Reemplaza con tu SSID de WiFi!
const char* password = "sochoagu"; // ¡Reemplaza con tu contraseña de WiFi!

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

std::map<String, Tarjeta>* globalTarjetasActivasPtr;

// Declaración forward de la función para eliminar (si no está ya en Tarjeta.h o utils.h)
// Esto asume que main.cpp tendrá una función como 'removeCardByUid'
// o que la lógica de eliminación se hará directamente aquí en webserver.cpp
// Si la lógica está en main.cpp, necesitarás una referencia a la función.
// Para simplificar, la haremos aquí mismo ya que tenemos el puntero al mapa.
void removeCardFromMap(const String& uid_to_remove);


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
      // Manejar datos recibidos del cliente
      {
        AwsFrameInfo *info = (AwsFrameInfo*)arg;
        if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
          data[len] = 0; // Asegurarse de que sea un string nulo
          String message = (char*)data;
          Serial.printf("Mensaje recibido de cliente #%u: %s\n", client->id(), message.c_str());

          // Parsear el JSON recibido
          StaticJsonDocument<200> doc; // Tamaño suficiente para {"action": "delete", "uid": "B1616C803"}
          DeserializationError error = deserializeJson(doc, message);

          if (error) {
            Serial.print(F("deserializeJson() failed: "));
            Serial.println(error.f_str());
            return;
          }

          const char* action = doc["action"];
          if (action && strcmp(action, "delete") == 0) {
            const char* uidToDelete = doc["uid"];
            if (uidToDelete) {
              Serial.printf("Solicitud de eliminación de tarjeta: %s\n", uidToDelete);
              removeCardFromMap(uidToDelete); // Eliminar la tarjeta
              sendWebSocketData(); // Enviar actualización a todos los clientes después de eliminar
            } else {
              Serial.println("UID de tarjeta no especificado para la acción de eliminación.");
            }
          } else {
            Serial.println("Acción desconocida en el mensaje WebSocket.");
          }
        }
      }
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

// Función para eliminar una tarjeta del mapa
void removeCardFromMap(const String& uid_to_remove) {
    auto it = globalTarjetasActivasPtr->find(uid_to_remove);
    if (it != globalTarjetasActivasPtr->end()) {
        globalTarjetasActivasPtr->erase(it);
        Serial.printf("Tarjeta %s eliminada del mapa.\n", uid_to_remove.c_str());
    } else {
        Serial.printf("Error: Tarjeta %s no encontrada para eliminar.\n", uid_to_remove.c_str());
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
    std::vector<const Tarjeta*> sortedTarjetas;
    for (auto const& pair : *globalTarjetasActivasPtr) {
      sortedTarjetas.push_back(&pair.second);
    }

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