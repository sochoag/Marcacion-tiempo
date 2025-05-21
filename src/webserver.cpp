#include "webserver.h"
#include "utils.h"
#include <FS.h>
#include <SPIFFS.h>
#include <vector>
#include <algorithm>
#include <ArduinoJson.h> // Necesario para parsear JSON
#include <WiFi.h> // Necesario para funciones WiFi.softAP

// >>> CONFIGURACIÓN PARA ESP32 COMO HOTSPOT (ACCESS POINT) <<<
// ¡¡¡AJUSTA ESTOS VALORES!!!
const char* ap_ssid = "ESP32_RFID_AP";     // Nombre de la red Wi-Fi que creará el ESP32
const char* ap_password = "sochoagu";  // Contraseña para conectarse a la red del ESP32 (mínimo 8 caracteres)

IPAddress ap_local_ip(192, 168, 4, 1);    // IP del ESP32 en modo AP (¡Esta es la IP a la que te conectarás!)
IPAddress ap_gateway(192, 168, 4, 1);    // Gateway para los clientes (será la misma IP del ESP32)
IPAddress ap_subnet(255, 255, 255, 0);   // Máscara de subred para la red AP
// >>> FIN CONFIGURACIÓN HOTSPOT <<<


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
          const char* uid = doc["uid"]; // Ahora también procesamos el UID para "liberate"
          
          if (action) {
            if (strcmp(action, "delete") == 0) {
              if (uid) {
                Serial.printf("Solicitud de eliminación de tarjeta: %s\n", uid);
                removeCardFromMap(uid); // Eliminar la tarjeta
                sendWebSocketData(); // Enviar actualización a todos los clientes después de eliminar
              } else {
                Serial.println("UID de tarjeta no especificado para la acción de eliminación.");
              }
            } else if (strcmp(action, "liberate") == 0) { // Manejar la acción de "liberate"
              if (uid) {
                // Aquí, en lugar de eliminar, necesitarías una lógica para "liberar" la tarjeta.
                // Como en el frontend, esto podría implicar marcarla como inactiva/liberada
                // en el mapa, pero sin borrarla para que persista en el historial.
                // Si la intención es que el ESP32 olvide completamente esa sesión para que pueda iniciar una nueva,
                // la acción "delete" ya lo hace. Si "liberate" significa un estado diferente
                // que el ESP32 debe reconocer (ej. no iniciar un nuevo tiempo si se lee),
                // necesitarás ajustar la lógica de `Tarjeta` o `utils.cpp`.
                // Por ahora, y dado que el front-end solo envía "delete" para liberar,
                // podemos llamar a removeCardFromMap aquí también si esa es la intención del backend.
                // O si quieres que el ESP32 persista la sesión pero la marque como "liberada",
                // necesitarías un método en Tarjeta para eso y actualizar el mapa.
                // Basado en la discusión anterior, "liberar" en el frontend envía "delete" al backend
                // para que el ESP32 la olvide y pueda crear una nueva sesión si se vuelve a leer.
                Serial.printf("Solicitud de liberación (eliminar del ESP32): %s\n", uid);
                removeCardFromMap(uid); 
                sendWebSocketData(); 
              } else {
                Serial.println("UID de tarjeta no especificado para la acción de liberación.");
              }
            } else {
              Serial.println("Acción desconocida en el mensaje WebSocket.");
            }
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

  // Configurar el ESP32 en modo Access Point (AP)
  Serial.print("Configurando ESP32 como Hotspot: ");
  Serial.println(ap_ssid);

  // Es buena práctica configurar la IP antes de iniciar el AP
  // Esto asegura que el AP use la IP que queremos desde el principio
  WiFi.softAPConfig(ap_local_ip, ap_gateway, ap_subnet);

  // Iniciar el Access Point
  // WiFi.softAP(ap_ssid, ap_password)
  if (WiFi.softAP(ap_ssid, ap_password)) {
    Serial.println("Hotspot AP creado exitosamente.");
    
    Serial.print("Dirección IP del Hotspot (Gateway): ");
    Serial.println(WiFi.softAPIP()); // Esta es la IP a la que te conectarás

    ws.onEvent(onWsEvent);
    server.addHandler(&ws);

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
      request->send(SPIFFS, "/index.html", "text/html");
    });

    server.onNotFound([](AsyncWebServerRequest *request){
      request->send(404, "text/plain", "Not Found");
    });

    server.begin();
    Serial.println("Servidor web iniciado en el Hotspot");
  } else {
    Serial.println("¡ERROR! Fallo al crear el Hotspot AP. Revisa los parámetros.");
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