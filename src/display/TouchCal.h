#ifndef SOFAR_TOUCH_CAL_H
#define SOFAR_TOUCH_CAL_H

#include <stdint.h>

// Resistive-touch calibration. The XPT2046 reports a 12-bit ratio of a
// resistive divider, so the usable range is specific to the physical panel,
// and the overlay's axis order/direction is independent of the TFT rotation.
// Both are therefore measured, never assumed.
//
// The wizard samples three targets, chosen so each pair differs in exactly
// one screen axis:
//   A = (LO_X, LO_Y)   B = (HI_X, LO_Y)   C = (LO_X, HI_Y)
// A->B moves screen X only, so whichever raw axis changes more is the one
// driving screen X. A->C does the same for Y. A negative span means the axis
// is inverted, which needs no extra flag.

#define TCAL_SCREEN_W  240
#define TCAL_SCREEN_H  320
#define TCAL_LO_X       24
#define TCAL_HI_X      216
#define TCAL_LO_Y       48
#define TCAL_HI_Y      288
#define TCAL_MIN_SPAN  300   // raw counts; less than this means noise, not travel

struct TouchSample { int16_t x, y; };

struct TouchCal {
    bool    valid = false;
    bool    swap  = false;   // true: raw Y drives screen X
    int16_t xLo   = 0;       // raw reading of the X-driving axis at TCAL_LO_X
    int16_t xHi   = 0;       // ... at TCAL_HI_X
    int16_t yLo   = 0;       // raw reading of the Y-driving axis at TCAL_LO_Y
    int16_t yHi   = 0;       // ... at TCAL_HI_Y
};

inline int32_t tcalAbs(int32_t v) { return v < 0 ? -v : v; }

// Returns false if the samples cannot describe a usable panel: too little
// travel (noise, saturated reads, or the user tapping the same spot), or both
// screen axes tracking the same raw axis (dead channel).
inline bool touchCalBuild(TouchSample a, TouchSample b, TouchSample c,
                          TouchCal& out)
{
    int32_t dbx = (int32_t)b.x - a.x, dby = (int32_t)b.y - a.y;  // screen X moved
    int32_t dcx = (int32_t)c.x - a.x, dcy = (int32_t)c.y - a.y;  // screen Y moved

    bool swap    = tcalAbs(dby) > tcalAbs(dbx);
    int32_t xSpan = swap ? dby : dbx;
    int32_t ySpan = swap ? dcx : dcy;
    if (tcalAbs(xSpan) < TCAL_MIN_SPAN || tcalAbs(ySpan) < TCAL_MIN_SPAN)
        return false;
    // The Y target must move the *other* raw axis, else one channel is stuck.
    if (tcalAbs(ySpan) <= tcalAbs(swap ? dcy : dcx)) return false;

    out.swap  = swap;
    out.xLo   = swap ? a.y : a.x;
    out.xHi   = swap ? b.y : b.x;
    out.yLo   = swap ? a.x : a.y;
    out.yHi   = swap ? c.x : c.y;
    out.valid = true;
    return true;
}

inline int16_t tcalClamp(int32_t v, int32_t hi) {
    if (v < 0)  return 0;
    if (v > hi) return (int16_t)hi;
    return (int16_t)v;
}

// Maps a raw reading to screen pixels. Extrapolates past the targets, so the
// screen edges outside the calibration rectangle stay reachable.
inline void touchCalApply(const TouchCal& cal, int16_t rawX, int16_t rawY,
                          int16_t& sx, int16_t& sy)
{
    int32_t rx = cal.swap ? rawY : rawX;
    int32_t ry = cal.swap ? rawX : rawY;
    int32_t xDen = (int32_t)cal.xHi - cal.xLo;
    int32_t yDen = (int32_t)cal.yHi - cal.yLo;
    if (xDen == 0 || yDen == 0) { sx = sy = -1; return; }

    int32_t px = TCAL_LO_X + (rx - cal.xLo) * (TCAL_HI_X - TCAL_LO_X) / xDen;
    int32_t py = TCAL_LO_Y + (ry - cal.yLo) * (TCAL_HI_Y - TCAL_LO_Y) / yDen;
    sx = tcalClamp(px, TCAL_SCREEN_W - 1);
    sy = tcalClamp(py, TCAL_SCREEN_H - 1);
}

#endif // SOFAR_TOUCH_CAL_H
