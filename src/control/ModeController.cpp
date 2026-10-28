#include "control/ModeController.h"
#include "util/AppLog.h"

void ModeController::persist() {
    _cfg.setMode(::modeName(_mode));
    _cfg.setChargePower(_chargePower);
    _cfg.setAutoLimit(_autoLimit);
    _cfg.save();   // dirty-checked: a repeated command commits nothing
}

void ModeController::restore() {
    _chargePower = _cfg.chargePower();
    _autoLimit   = _cfg.autoLimit() > 0 ? _cfg.autoLimit() : AUTO_LIMIT_DEFAULT;
    Mode m = Mode::Auto;
    char lb[64];
    if (!modeParse(_cfg.mode(), m)) {
        snprintf(lb, sizeof(lb), "Unknown stored mode '%s', using auto", _cfg.mode());
    } else {
        snprintf(lb, sizeof(lb), "Restoring mode '%s'", ::modeName(m));
    }
    appLog.add("CTL", lb);
    setMode(m);
}

void ModeController::tick() {
    if (_mode == Mode::BatterySaver) _bs.update();
    else                             reconcile();
}

void ModeController::reconcile() {
    if (_inv.hasError()) return;
    int32_t minP = 0, maxP = 0;
    if (modeDesiredPassiveRange(_mode, _chargePower, _autoLimit, minP, maxP))
        _inv.sendPassiveRange(minP, maxP);   // write cache drops unchanged ranges
}

// BatterySaver::disable() never writes on its own: the next mode's single
// range write is the transition, so battery_saver -> charge is one write.
void ModeController::setMode(Mode m) {
    if (m != _mode) {
        char lb[40];
        snprintf(lb, sizeof(lb), "mode %s -> %s", ::modeName(_mode), ::modeName(m));
        appLog.add("CTL", lb);
    }
    if (m == Mode::BatterySaver) {
        if (!_bs.isActive()) _bs.enable();   // a repeated "on" must not re-probe
    } else {
        _bs.disable();
    }
    _mode = m;
    reconcile();
    persist();
}

void ModeController::setCharge(int32_t watts) {
    _chargePower = watts;
    setMode(Mode::Charge);
}

void ModeController::setAuto(int32_t limit) {
    _autoLimit = limit > 0 ? limit : AUTO_LIMIT_DEFAULT;
    setMode(Mode::Auto);
}

void ModeController::toggleBatterySaver() {
    Command c;
    c.kind = CmdKind::SaverToggle;
    apply(c);
}

void ModeController::apply(const Command& c) {
    switch (c.kind) {
    case CmdKind::SetMode:     setMode(c.mode); break;
    case CmdKind::SaverSwitch: setMode(modeForSaverSwitch(_mode, c.on)); break;
    case CmdKind::SaverToggle: setMode(modeForSaverSwitch(_mode, !saverActive())); break;
    case CmdKind::SetCharge:   setCharge(c.value); break;
    case CmdKind::SetAuto:     setAuto(c.value); break;
    }
}
