#ifndef SOFAR_BATTERY_SAVER_H
#define SOFAR_BATTERY_SAVER_H

#include "inverter/Inverter.h"
#include "Config.h"

class BatterySaver {
public:
    explicit BatterySaver(Inverter& inv) : _inv(inv) {}

    void enable();
    void disable();
    void toggle();

    // Call every loop iteration.  Internally rate-limited to INTERVAL_BSAVE.
    void update();

    bool    isActive()    const { return _active; }
    int32_t targetPower() const { return _targetPower; }

private:
    Inverter& _inv;
    bool      _active      = false;
    int32_t   _targetPower = 0;
    unsigned long _lastRun = 0;
};

#endif // SOFAR_BATTERY_SAVER_H
