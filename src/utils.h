#ifndef UTILS_H
#define UTILS_H

#include <Arduino.h> // Para String y unsigned long

// Declaración de tu función existente
String toHexString(byte *uid, byte size);

// Declaración de la nueva función
String formatMillisToMMSS(unsigned long millis);

#endif