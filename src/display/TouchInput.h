#ifndef SOFAR_TOUCH_INPUT_H
#define SOFAR_TOUCH_INPUT_H

#include <XPT2046_Touchscreen.h>
#include "display/TouchCal.h"

// XPT2046 gestures and the 3-point calibration wizard. Knows nothing about
// what is on screen: it reports events, Display decides what they mean.
//
// Gesture-based, so it cannot be broken by panel rotation/wiring:
//   press                -> wake if the backlight is off (consumes the gesture)
//   release before 800ms -> short tap (with screen coords when calibrated)
//   held past 800ms      -> long press, fires once per press
//   held 4 s             -> calibration wizard
class TouchInput {
public:
    enum Event : uint8_t {
        TE_WAKE       = 1 << 0,
        TE_TAP        = 1 << 1,
        TE_LONG       = 1 << 2,
        TE_HINT       = 1 << 3,   // calibration-hold notice toggled
        TE_CAL_START  = 1 << 4,
        TE_CAL_REDRAW = 1 << 5,   // wizard advanced or restarted
        TE_CAL_DONE   = 1 << 6,
    };

    TouchInput(uint8_t csPin, uint8_t irqPin) : _ts(csPin, irqPin) {}

    void begin();
    uint8_t poll(bool screenOn);   // Event bits; at most one poll per 30 ms

    void startCalibration();
    bool calibrating() const    { return _calStep < CAL_DONE; }
    uint8_t calStep() const     { return _calStep; }
    bool calRetry() const       { return _calRetry; }
    bool calNotice() const      { return _calNotice; }
    void setCal(const TouchCal& c) { _cal = c; }
    const TouchCal& cal() const { return _cal; }
    void onCalibrated(void (*cb)(const TouchCal&)) { _calSaved = cb; }

    int16_t tapX() const { return _tapX; }   // -1 when uncalibrated
    int16_t tapY() const { return _tapY; }
    unsigned long lastTouchAt() const { return _lastTouch; }

private:
    enum CalStep : uint8_t { CAL_A = 0, CAL_B, CAL_C, CAL_DONE };

    XPT2046_Touchscreen _ts;
    bool          _touchedPrev  = false;
    unsigned long _lastTouch    = 0;
    unsigned long _pressStart   = 0;
    unsigned long _lastPollAt   = 0;
    unsigned long _lastDownAt   = 0;
    bool          _calNotice    = false;
    bool          _longFired    = false;
    int16_t       _tapX         = -1;
    int16_t       _tapY         = -1;
    char          _dbg[48]      = "";

    TouchCal      _cal;
    uint8_t       _calStep        = CAL_DONE;
    bool          _calRetry       = false;
    bool          _calIgnorePress = false;
    TouchSample   _calRaw[3]      = {};
    int32_t       _calSumX        = 0;
    int32_t       _calSumY        = 0;
    int16_t       _calCount       = 0;
    void (*_calSaved)(const TouchCal&) = nullptr;

    uint8_t calPoll(bool down, unsigned long now);
};

#endif // SOFAR_TOUCH_INPUT_H
