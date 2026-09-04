#include "BatterySaver.h"
#include "Config.h"
#include "SaverAlgorithm.h"

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

void BatterySaver::enable() {
    _active      = true;
    _targetPower = 0;
    // Force the first update() to write, whatever the inverter cache holds
    _lastSentTarget = -1;
    _lastSentAt     = 0;
    _zeroStart      = 0;
}

void BatterySaver::disable() {
    _active      = false;
    _targetPower = 0;
    // Return inverter to standby so it stops forced-charge
    _inv.sendPassiveCommand(0);
}

void BatterySaver::toggle() {
    _active ? disable() : enable();
}

// Charges the battery from excess solar: grid export raises the charge
// target, import lowers it (clamped so the battery never discharges).
// Write-suppression policy lives in SaverAlgorithm.h.
void BatterySaver::update() {
    if (!_active) return;

    const InverterData& d = _inv.data();
    if (!d.valid) return;

    unsigned long now = millis();

    _targetPower = saverAccumulateTarget(_targetPower, d.gridPower, _maxPower);

    if (_targetPower == 0) {
        if (_zeroStart == 0) _zeroStart = now;
    } else {
        _zeroStart = 0;
    }

    if (!saverShouldSend(_targetPower, _lastSentTarget, now, _lastSentAt,
                         _zeroStart, _minDelta, _inv.keepaliveMs(),
                         _idleLapseMs)) {
        return;
    }

    if (_inv.sendPassiveCommand(_targetPower)) {
        _lastSentTarget = _targetPower;
        _lastSentAt     = now;
    }
}
