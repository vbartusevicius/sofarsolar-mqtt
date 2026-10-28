#include "display/TouchInput.h"
#include "display/TouchGesture.h"
#include "util/AppLog.h"

#define TS_POLL_MS     30
#define TS_LONG_MS    TG_LONG_MS
#define TS_CAL_MS     TG_CAL_MS   // hold this long to start touch calibration
#define TS_MIN_TAP_MS TG_MIN_TAP_MS

void TouchInput::begin() {
    _ts.begin();
    _ts.setRotation(1);
    _lastTouch = millis();
}

void TouchInput::startCalibration() {
    _calStep  = CAL_A;
    _calRetry = false;
    _cal.valid = false;
    // If the wizard was started by a long press, the finger is still down:
    // that press must not be mistaken for the first target tap.
    _calIgnorePress = _touchedPrev;
    _calNotice      = false;
    appLog.add("TCH", "calibration started");
}

uint8_t TouchInput::poll(bool screenOn) {
    unsigned long now = millis();
    if (now - _lastPollAt < TS_POLL_MS) return 0;
    _lastPollAt = now;

    uint32_t lastDown = (uint32_t)_lastDownAt;
    bool down = touchDebouncedDown(_ts.touched(), _touchedPrev, (uint32_t)now,
                                   lastDown);
    _lastDownAt = lastDown;

    if (calibrating()) return calPoll(down, now);

    if (down) {
        _lastTouch = now;
        if (!_touchedPrev) {                       // press
            _touchedPrev = true;
            _pressStart  = now;
            _longFired   = false;
            _tapX = _tapY = -1;
            if (!screenOn) { _longFired = true; return TE_WAKE; }
            TS_Point p = _ts.getPoint();
            if (_cal.valid) {
                touchCalApply(_cal, p.x, p.y, _tapX, _tapY);
                snprintf(_dbg, sizeof(_dbg), "raw %d,%d -> %d,%d",
                         (int)p.x, (int)p.y, (int)_tapX, (int)_tapY);
            } else {
                snprintf(_dbg, sizeof(_dbg), "raw %d,%d (uncal)",
                         (int)p.x, (int)p.y);
            }
            appLog.add("TCH", _dbg);
            return 0;
        }
        uint8_t ev = 0;
        if (!_longFired && now - _pressStart >= TS_LONG_MS) {
            _longFired = true;
            ev |= TE_LONG;
        }
        if (!_calNotice && now - _pressStart >= TS_CAL_MS / 2) {
            _calNotice = true;
            ev |= TE_HINT;
        }
        if (now - _pressStart >= TS_CAL_MS) {
            startCalibration();
            ev |= TE_CAL_START;
        }
        return ev;
    }

    if (!_touchedPrev) return 0;
    _touchedPrev = false;                          // release
    uint8_t ev = 0;
    if (_calNotice) { _calNotice = false; ev |= TE_HINT; }
    if (!_longFired && (now - _pressStart) >= TS_MIN_TAP_MS) ev |= TE_TAP;
    return ev;
}

// Averages the readings taken while the finger is down: a single sample is
// noisy, and the first few counts arrive while contact is still settling.
uint8_t TouchInput::calPoll(bool down, unsigned long now) {
    if (_calIgnorePress) {
        if (down) { _lastTouch = now; return 0; }
        _calIgnorePress = false;
        _touchedPrev    = false;
        return 0;
    }
    if (down) {
        _lastTouch = now;
        if (!_touchedPrev) {
            _touchedPrev = true;
            _pressStart  = now;
            _calSumX = _calSumY = 0;
            _calCount = 0;
            return 0;
        }
        if (now - _pressStart >= 60) {          // let the contact settle first
            TS_Point p = _ts.getPoint();
            _calSumX += p.x;
            _calSumY += p.y;
            _calCount++;
        }
        return 0;
    }
    if (!_touchedPrev) return 0;
    _touchedPrev = false;
    if (_calCount < 2) return 0;                 // too brief to trust

    _calRaw[_calStep].x = (int16_t)(_calSumX / _calCount);
    _calRaw[_calStep].y = (int16_t)(_calSumY / _calCount);
    snprintf(_dbg, sizeof(_dbg), "cal %d raw %d,%d n=%d",
             _calStep + 1, (int)_calRaw[_calStep].x, (int)_calRaw[_calStep].y,
             (int)_calCount);
    appLog.add("TCH", _dbg);

    if (_calStep < CAL_C) { _calStep++; return TE_CAL_REDRAW; }

    TouchCal built;
    if (!touchCalBuild(_calRaw[0], _calRaw[1], _calRaw[2], built)) {
        appLog.add("TCH", "calibration rejected: no usable travel");
        _calStep  = CAL_A;
        _calRetry = true;
        return TE_CAL_REDRAW;
    }
    _cal     = built;
    _calStep = CAL_DONE;
    snprintf(_dbg, sizeof(_dbg), "cal ok swap=%u x %d..%d y %d..%d",
             (unsigned)(_cal.swap ? 1 : 0), (int)_cal.xLo, (int)_cal.xHi,
             (int)_cal.yLo, (int)_cal.yHi);
    appLog.add("TCH", _dbg);
    if (_calSaved) _calSaved(_cal);
    return TE_CAL_DONE;
}
