#include "BatterySaver.h"

void BatterySaver::enable() {
    _active      = true;
    _targetPower = 0;
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

// ── Core loop ──────────────────────────────────────────────────
// Reads grid power every INTERVAL_BSAVE ms and adjusts the
// charge target so the battery absorbs only excess solar.
//
// Positive gridPower = exporting  → room to charge more
// Negative gridPower = importing  → must reduce charge
//
// The target is accumulated and clamped to [0, BSAVE_MAX_POWER]
// so the battery NEVER discharges.  Even when target is 0 W the
// command is still sent as a keep-alive (inverter times out after
// ~60 s without a write).
void BatterySaver::update() {
    if (!_active) return;

    unsigned long now = millis();
    if (_lastRun > now) _lastRun = 0;              // millis() overflow
    if (now - _lastRun < INTERVAL_BSAVE) return;
    _lastRun = now;

    const InverterData& d = _inv.data();
    if (!d.valid) return;

    // Accumulate: positive grid power (export) → increase charge target
    _targetPower += d.gridPower;

    // Clamp
    if (_targetPower < 0)                _targetPower = 0;
    if (_targetPower > BSAVE_MAX_POWER)  _targetPower = BSAVE_MAX_POWER;

    // Always send (keep-alive), even if 0 W
    _inv.sendPassiveCommand(_targetPower);
}
