#include "webserver.h"
#include "utils.h" // Para el formato de tiempo
#include <FS.h>      // Para el sistema de archivos SPIFFS
#include <SPIFFS.h>  // Para usar SPIFFS directamente

const char* ssid = "sochoag";       // ¡Reemplaza con tu SSID de WiFi!
const char* password = "sochoagu"; // ¡Reemplaza con tu contraseña de WiFi!

AsyncWebServer server(80);
AsyncWebSocket ws("/ws"); // Servidor WebSocket en la ruta "/ws"

// Puntero para acceder al mapa de tarjetas
std::map<String, Tarjeta>* globalTarjetasActivasPtr;

// Función para manejar eventos del WebSocket (conexión, desconexión, mensajes)
void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      Serial.printf("Cliente WebSocket conectado #%u desde %s\n", client->id(), client->remoteIP().toString().c_str());
      // Cuando un nuevo cliente se conecta, enviarle el estado actual de la tabla
      sendWebSocketData();
      break;
    case WS_EVT_DISCONNECT:
      Serial.printf("Cliente WebSocket desconectado #%u\n", client->id());
      break;
    case WS_EVT_DATA:
      // Aquí podrías manejar mensajes recibidos del cliente (ej. comandos)
      Serial.printf("Mensaje recibido de cliente #%u: %s\n", client->id(), (char*)data);
      break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
  }
}

// Función para inicializar el servidor web y WebSocket
void initWebServer(std::map<String, Tarjeta>& tarjetas) {
  globalTarjetasActivasPtr = &tarjetas; // Asigna la dirección del mapa

  // Inicializar SPIFFS
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

    // Configurar el manejador del WebSocket
    ws.onEvent(onWsEvent);
    server.addHandler(&ws); // Añadir el WebSocket al servidor web

    // Servir archivos estáticos desde SPIFFS (por ejemplo, index.html)
    // Cuando el navegador pida la ruta raíz "/", enviamos el archivo "/index.html" del SPIFFS
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
      request->send(SPIFFS, "/index.html", "text/html");
    });

    // Manejar archivos no encontrados
    server.onNotFound([](AsyncWebServerRequest *request){
      request->send(404, "text/plain", "Not Found");
    });

    server.begin(); // Inicia el servidor HTTP
    Serial.println("Servidor web iniciado");
  } else {
    Serial.println("\nFallo al conectar a WiFi. Por favor, revisa tu SSID y contraseña.");
  }
}

// Función para enviar los datos de las tarjetas a todos los clientes WebSocket conectados
// Función para enviar los datos de las tarjetas a todos los clientes WebSocket conectados
void sendWebSocketData() {
  if (globalTarjetasActivasPtr->empty()) {
    ws.textAll("[]");
    return;
  }

  String jsonArray = "[";
  bool firstEntry = true;
  for (auto const& pair : *globalTarjetasActivasPtr) {
    const Tarjeta& tarjeta = pair.second;

    if (!firstEntry) {
      jsonArray += ",";
    }
    firstEntry = false;

    jsonArray += "{";
    jsonArray += "\"uid\": \"" + tarjeta.getUid() + "\",";
    // ¡Aquí es donde incluimos el ID de grupo!
    jsonArray += "\"grupo\": \"Grupo " + String(tarjeta.getGroupId()) + "\",";
    
    String totalTimeFormatted;
    if (tarjeta.isActive()) {
      totalTimeFormatted = formatMillisToMMSS(millis() - tarjeta.getStartTime());
    } else {
      totalTimeFormatted = tarjeta.getTotalTime();
    }
    jsonArray += "\"tiempoTotal\": \"" + totalTimeFormatted + "\"";
    jsonArray += "}";
  }
  jsonArray += "]";

  ws.textAll(jsonArray);
  Serial.println("Datos WebSocket enviados.");
}