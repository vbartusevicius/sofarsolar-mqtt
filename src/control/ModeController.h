#ifndef SOFAR_MODE_CONTROLLER_H
#define SOFAR_MODE_CONTROLLER_H

#include <Arduino.h>
#include "inverter/Inverter.h"
#include "control/BatterySaver.h"
#include "config/EEConfig.h"

class ModeController {
public:
    ModeController(Inverter& inv, BatterySaver& bs, EEConfig& cfg)
        : _inv(inv), _bs(bs), _cfg(cfg) {}

    // Re-applies the persisted mode after a reboot. Call once the Modbus link
    // is up, since restoring a mode means commanding the inverter.
    void restore();

    void setMode(const char* mode);
    void setCharge(int32_t watts);
    void setAuto(int32_t limit);
    void toggleBatterySaver();

    const char* currentMode() const { return _mode; }
    int32_t chargePower()     const { return _chargePower; }
    int32_t autoLimit()       const { return _autoLimit; }

private:
    Inverter&     _inv;
    BatterySaver& _bs;
    EEConfig&     _cfg;
    char    _mode[16]    = "auto";
    int32_t _chargePower = 0;
    int32_t _autoLimit   = 16384;

    void setModeName(const char* name);
    void persist();
};

#endif // SOFAR_MODE_CONTROLLER_H
