#ifndef SOFAR_MODE_CONTROLLER_H
#define SOFAR_MODE_CONTROLLER_H

#include <Arduino.h>
#include "inverter/Inverter.h"
#include "control/BatterySaver.h"

// Owns the inverter's control mode ("auto" | "charge" | "standby" |
// "battery_saver") and the associated setpoints.  All command paths
// (MQTT callbacks, web routes, touch button) go through here.
class ModeController {
public:
    ModeController(Inverter& inv, BatterySaver& bs) : _inv(inv), _bs(bs) {}

    void setMode(const char* mode);
    void setCharge(int32_t watts);
    void setAuto(int32_t limit);
    void toggleBatterySaver() { _bs.toggle(); }

    const char* currentMode() const { return _mode; }
    int32_t chargePower()     const { return _chargePower; }
    int32_t autoLimit()       const { return _autoLimit; }

private:
    Inverter&     _inv;
    BatterySaver& _bs;
    char    _mode[16]    = "auto";
    int32_t _chargePower = 0;
    int32_t _autoLimit   = 16384;

    void setModeName(const char* name);
};

#endif // SOFAR_MODE_CONTROLLER_H
