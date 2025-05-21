#ifndef Tarjeta_h
#define Tarjeta_h

#include <Arduino.h>

class Tarjeta {
public:
    // Constructor modificado para aceptar un ID de grupo
    Tarjeta(String uid, unsigned long startTime, int groupId);

    String getUid() const;
    int getGroupId() const; // Nuevo método para obtener el ID de grupo

    void setEndTime(unsigned long endTime);
    unsigned long getEndTime() const;
    unsigned long getStartTime() const;
    String getTotalTime() const;
    bool isActive() const;

private:
    String _uid;
    unsigned long _startTime;
    unsigned long _endTime;
    bool _isActive;
    int _groupId; // Nueva propiedad para el ID del grupo
};

#endif