#include <Arduino.h>
#include <SPI.h>
#include <MFRC522.h>
#include <map>
#include <string>

#include "utils.h"
#include "Tarjeta.h"
#include "webserver.h"

#define SS_PIN 5
#define RST_PIN 22

// RFID reader instance
MFRC522 rfid(SS_PIN, RST_PIN);
MFRC522::MIFARE_Key rfidKey;

// Lista de tarjetas activas
std::map<String, Tarjeta> tarjetasActivas;

// Contador para asignar IDs de grupo
int nextGroupId = 1;

void setup() {
  Serial.begin(115200);
  SPI.begin();
  rfid.PCD_Init();

  for (byte i = 0; i < MFRC522::MF_KEY_SIZE; i++) {
    rfidKey.keyByte[i] = 0xFF;
  }

  Serial.println(F("Scan PICC to see UID"));
  rfid.PCD_DumpVersionToSerial();

  initWebServer(tarjetasActivas);
}

void loop() {
  if (!rfid.PICC_IsNewCardPresent()) return;
  if (!rfid.PICC_ReadCardSerial()) return;

  rfid.PICC_HaltA();

  Serial.print(F("Readed Card UID:"));
  String current_uid = toHexString(rfid.uid.uidByte, rfid.uid.size);
  printf("%s\n", current_uid.c_str());

  auto it = tarjetasActivas.find(current_uid);
  bool dataUpdated = false;

  if (it == tarjetasActivas.end()) {
    // Si es una tarjeta nueva, asignamos el siguiente ID de grupo
    printf("Nueva tarjeta cargada: %s con Grupo %d\n", current_uid.c_str(), nextGroupId);
    tarjetasActivas.insert({current_uid, Tarjeta(current_uid, millis(), nextGroupId)});
    nextGroupId++; // Incrementamos el contador para el próximo grupo
    printf("Total de tarjetas activas: %d\n", tarjetasActivas.size());
    dataUpdated = true;
  } else {
    // Si la tarjeta ya existe
    if (it->second.isActive()) {
      Serial.printf("Tarjeta activa encontrada: %s, Grupo %d\n", it->second.getUid().c_str(), it->second.getGroupId());
      it->second.setEndTime(millis());
      Serial.printf("Tiempo total: %s\n", it->second.getTotalTime().c_str());
      dataUpdated = true;
    } else {
      return;
      // Si la tarjeta ya fue 'salida' (no activa), la volvemos a 'entrar'
      // Reutilizamos el mismo ID de grupo que ya tenía
      // int existingGroupId = it->second.getGroupId(); // Obtenemos el grupo existente
      // Serial.printf("Tarjeta %s re-leida. Grupo %d. Reiniciando tiempo.\n", it->second.getUid().c_str(), existingGroupId);
      // tarjetasActivas.erase(it); // Eliminar la entrada anterior
      // tarjetasActivas.insert({current_uid, Tarjeta(current_uid, millis(), existingGroupId)}); // Crear nueva entrada con el mismo grupo
      // dataUpdated = true;
    }
  }

  // Si los datos de las tarjetas han cambiado, enviar por WebSocket
  if (dataUpdated) {
    sendWebSocketData();
  }
}