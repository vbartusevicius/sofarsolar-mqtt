#include "control/ModeController.h"
#include "control/SaverAlgorithm.h"
#include "util/AppLog.h"

void ModeController::setModeName(const char* name) {
    strncpy(_mode, name, sizeof(_mode) - 1);
    _mode[sizeof(_mode) - 1] = '\0';
}

void ModeController::persist() {
    _cfg.setMode(_mode);
    _cfg.setChargePower(_chargePower);
    _cfg.setAutoLimit(_autoLimit);
    _cfg.save();
}

void ModeController::toggleBatterySaver() {
    setMode(_bs.isActive() ? "standby" : "battery_saver");
}

void ModeController::restore() {
    _chargePower = _cfg.chargePower();
    _autoLimit   = _cfg.autoLimit();
    char lb[48];
    snprintf(lb, sizeof(lb), "Restoring mode '%s'", _cfg.mode());
    appLog.add("CTL", lb);
    setMode(_cfg.mode());
}

void ModeController::reconcile() {
    if (_inv.hasError()) return;
    int32_t minP = 0, maxP = 0;
    if (modeDesiredPassiveRange(_mode, _chargePower, _autoLimit, minP, maxP)) {
        _inv.sendPassiveRange(minP, maxP);
    }
}

void ModeController::setMode(const char* mode) {
    if (strcmp(mode, "battery_saver") == 0) {
        _bs.enable();
        setModeName("battery_saver");
    }
    else if (strcmp(mode, "charge") == 0) {
        _bs.disable();
        _inv.sendPassiveCommand(_chargePower);
        setModeName("charge");
    }
    else if (strcmp(mode, "standby") == 0) {
        _bs.disable();
        _inv.sendPassiveCommand(0);
        setModeName("standby");
    }
    else if (strcmp(mode, "auto") == 0) {
        setAuto(_autoLimit);
        return;
    }
    else {
        char lb[48];
        snprintf(lb, sizeof(lb), "Unknown mode '%s', ignored", mode);
        appLog.add("CTL", lb);
        return;
    }
    persist();
}

void ModeController::setCharge(int32_t watts) {
    _bs.disable();
    _chargePower = watts;
    _inv.sendPassiveCommand(watts);
    setModeName("charge");
    persist();
}

void ModeController::setAuto(int32_t limit) {
    _bs.disable();
    if (limit <= 0) limit = 16384;
    _autoLimit = limit;
    _inv.sendPassiveRange(-limit, limit);
    setModeName("auto");
    persist();
}
