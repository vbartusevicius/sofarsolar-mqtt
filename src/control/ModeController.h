#ifndef SOFAR_MODE_CONTROLLER_H
#define SOFAR_MODE_CONTROLLER_H

#include <Arduino.h>
#include "inverter/Inverter.h"
#include "control/BatterySaver.h"
#include "control/Command.h"
#include "control/Mode.h"
#include "config/EEConfig.h"

// Owns the control mode. Every surface (LCD, web, MQTT) goes through apply()
// or the setters here; BatterySaver is driven only from this class, so
// "is the saver running" and "is the mode battery_saver" cannot disagree.
class ModeController {
public:
    ModeController(Inverter& inv, BatterySaver& bs, EEConfig& cfg)
        : _inv(inv), _bs(bs), _cfg(cfg) {}

    // Re-applies the persisted mode after a reboot. The write itself waits
    // for the first successful poll (tick), so a sleeping inverter costs
    // nothing at boot.
    void restore();

    // One control step, called once per poll step: runs the battery saver
    // in battery_saver mode, otherwise makes sure the static mode's range
    // reached the inverter (retrying a write lost to a bus glitch).
    void tick();

    void apply(const Command& c);
    void setMode(Mode m);
    void setCharge(int32_t watts);
    void setAuto(int32_t limit);
    void toggleBatterySaver();

    Mode        mode()        const { return _mode; }
    const char* modeName()    const { return ::modeName(_mode); }
    bool        saverActive() const { return _mode == Mode::BatterySaver; }
    int32_t     chargePower() const { return _chargePower; }
    int32_t     autoLimit()   const { return _autoLimit; }

private:
    Inverter&     _inv;
    BatterySaver& _bs;
    EEConfig&     _cfg;
    Mode    _mode        = Mode::Auto;
    int32_t _chargePower = 0;
    int32_t _autoLimit   = AUTO_LIMIT_DEFAULT;

    void reconcile();
    void persist();
};

#endif // SOFAR_MODE_CONTROLLER_H
