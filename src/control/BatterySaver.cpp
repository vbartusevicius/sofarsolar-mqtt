#include "BatterySaver.h"
#include "Config.h"
#include "SaverAlgorithm.h"
#include "util/AppLog.h"

static int32_t clampI32(int32_t v, int32_t lo, int32_t hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

void BatterySaver::setMinDelta(int32_t w) {
    _minDelta = clampI32(w, 20, 2000);
}

void BatterySaver::setMaxPower(int32_t w) {
    _maxPower = clampI32(w, 0, BSAVE_MAX_POWER);
}

void BatterySaver::setIdleLapse(uint32_t ms) {
    _idleLapseMs = ms < 60000UL ? 60000UL : (ms > 3600000UL ? 3600000UL : ms);
}

const char* BatterySaver::strategyName() const {
    if (!_active) return "off";
    switch (_strategy) {
    case BSAVE_PROBING: return "probing";
    case BSAVE_NATIVE:  return "native";
    case BSAVE_LOOP:    return "loop";
    }
    return "off";
}

void BatterySaver::enable() {
    _active           = true;
    _targetPower      = 0;
    _lastSentTarget   = -1;
    _lastSentAt       = 0;
    _zeroTiming       = false;
    _lastSampleSeq    = 0;
    _strategy         = BSAVE_PROBING;
    _probeFailures    = 0;
    _nativeFailStreak = 0;
}

void BatterySaver::disable() {
    _active      = false;
    _targetPower = 0;
}

// Option A (preferred): write the native [Blo=0, Bup=_maxPower] window once so
// the inverter's own DSP tracks 0 W grid power without periodic RS485 writes.
// Accepted if the FC16 write is acknowledged, then verified by watching live
// power flow; if either check fails, falls back to Option B (the software
// accumulator loop).
void BatterySaver::update() {
    if (!_active) return;

    const InverterData& d = _inv.data();
    if (!d.valid) return;

    if (_strategy == BSAVE_PROBING) {
        // An FC16 ACK from 0x1187 confirms the inverter accepted the 6-register
        // [Gdes=0, Blo=0, Bup=_maxPower] write. Retry up to 3 times before
        // concluding the inverter rejected the asymmetric window, so a single
        // RS485 turnaround timeout at boot does not force software-loop mode.
        if (_inv.sendPassiveRange(0, _maxPower)) {
            _strategy         = BSAVE_NATIVE;
            _targetPower      = _maxPower;
            _lastSampleSeq    = d.sampleSeq;
            _nativeFailStreak = 0;
            appLog.add("BSV", "native [0..max] window active");
            return;
        }
        if (++_probeFailures < 3) return;
        _strategy       = BSAVE_LOOP;
        _targetPower    = 0;
        _lastSentTarget = -1;
        _lastSampleSeq  = 0;
        appLog.add("BSV", "native window write rejected 3x, using software loop");
    }

    if (_strategy == BSAVE_NATIVE) {
        // Re-asserts only if _maxPower changed in Web UI or keep-alive is due
        _inv.sendPassiveRange(0, _maxPower);
        _targetPower = _maxPower;
        if (saverNativeShouldFallback(d.gridPower, d.batteryPower, d.batterySOC,
                                      _maxPower, _minDelta, d.sampleSeq,
                                      _lastSampleSeq, _nativeFailStreak)) {
            _strategy       = BSAVE_LOOP;
            _targetPower    = 0;
            _lastSentTarget = -1;
            _lastSampleSeq  = 0;
            char lb[64];
            snprintf(lb, sizeof(lb), "native fallback: grid=%ld batt=%ld soc=%u",
                     (long)d.gridPower, (long)d.batteryPower, (unsigned)d.batterySOC);
            appLog.add("BSV", lb);
        } else {
            return;
        }
    }

    unsigned long now = millis();

    _targetPower = saverIntegrateSample(_targetPower, d.gridPower, _maxPower,
                                        d.sampleSeq, _lastSampleSeq);

    if (_targetPower == 0) {
        if (!_zeroTiming) { _zeroTiming = true; _zeroStart = now; }
    } else {
        _zeroTiming = false;
    }

    if (!saverShouldSend(_targetPower, _lastSentTarget, now, _lastSentAt,
                         _zeroTiming, _zeroStart, _minDelta,
                         _inv.keepaliveMs(), _idleLapseMs)) {
        return;
    }

    if (_inv.sendPassiveCommand(_targetPower)) {
        _lastSentTarget = _targetPower;
        _lastSentAt     = now;
    }
}
