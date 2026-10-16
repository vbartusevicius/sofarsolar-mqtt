#ifndef SOFAR_BATTERY_SAVER_H
#define SOFAR_BATTERY_SAVER_H

#include "Config.h"
#include "inverter/Inverter.h"

class BatterySaver {
public:
    enum Strategy : uint8_t {
        BSAVE_PROBING = 0,   // trying Option A: native [0, maxPower] window
        BSAVE_NATIVE  = 1,   // Option A verified on inverter
        BSAVE_LOOP    = 2,   // Option B fallback: software accumulator loop
    };

    explicit BatterySaver(Inverter& inv) : _inv(inv) {}

    void enable();
    void disable();
    void toggle();

    void update();

    bool        isActive()     const { return _active; }
    int32_t     targetPower()  const { return _targetPower; }
    Strategy    strategy()     const { return _strategy; }
    const char* strategyName() const;

    // Setters clamp to safe ranges; values persist via EEConfig
    void     setMinDelta(int32_t w);
    void     setMaxPower(int32_t w);
    void     setIdleLapse(uint32_t ms);
    int32_t  minDelta()    const { return _minDelta; }
    int32_t  maxPower()    const { return _maxPower; }
    uint32_t idleLapseMs() const { return _idleLapseMs; }

private:
    Inverter& _inv;
    bool      _active         = false;
    int32_t   _targetPower    = 0;
    int32_t   _lastSentTarget = -1;
    unsigned long _lastSentAt = 0;
    unsigned long _zeroStart  = 0;
    bool          _zeroTiming = false;   // 0 is a valid millis(), so flag it
    uint32_t      _lastSampleSeq = 0;
    Strategy      _strategy      = BSAVE_PROBING;
    uint8_t       _nativeFailStreak = 0;

    int32_t  _minDelta    = BSAVE_MIN_DELTA;
    int32_t  _maxPower    = BSAVE_MAX_POWER;
    uint32_t _idleLapseMs = BSAVE_IDLE_LAPSE_MS;
};

#endif
