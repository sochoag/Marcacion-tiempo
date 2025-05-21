#include "utils.h"

String toHexString(byte* buffer, byte size) {
    String result;
    result.reserve(size);
    for (byte i = 0; i < size; i++) {
        if (buffer[i] < 0x10) result += '0';
        result += String(buffer[i], HEX);
    }
    result.toUpperCase();
    return result;
}

String formatMillisToMMSS(unsigned long totalMillis) {
  unsigned long seconds = totalMillis / 1000;
  unsigned long minutes = seconds / 60;
  seconds = seconds % 60;

  // Formatea los minutos y segundos a String, asegurando dos dígitos con un cero inicial si es necesario
  String minutesStr = (minutes < 10) ? "0" + String(minutes) : String(minutes);
  String secondsStr = (seconds < 10) ? "0" + String(seconds) : String(seconds);

  return minutesStr + ":" + secondsStr;
}