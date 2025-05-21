#include "Tarjeta.h"
#include "utils.h"

// Constructor modificado
Tarjeta::Tarjeta(String uid, unsigned long startTime, int groupId) {
    _uid = uid;
    _startTime = startTime;
    _endTime = 0;
    _isActive = true;
    _groupId = groupId; // Inicializamos el ID de grupo
}

String Tarjeta::getUid() const {
    return _uid;
}

int Tarjeta::getGroupId() const { // Implementación del nuevo método
    return _groupId;
}

void Tarjeta::setEndTime(unsigned long endTime) {
    if(_isActive) {
      _endTime = endTime;
      _isActive = false;
    }
}

unsigned long Tarjeta::getEndTime() const {
    return _endTime;
}

unsigned long Tarjeta::getStartTime() const {
    return _startTime;
}

String Tarjeta::getTotalTime() const {
  unsigned long total_time = _endTime - _startTime;
  String total_time_formated = formatMillisToMMSS(total_time);
  return total_time_formated;
}

bool Tarjeta::isActive() const {
    return _isActive;
}