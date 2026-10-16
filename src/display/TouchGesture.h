#ifndef SOFAR_TOUCH_GESTURE_H
#define SOFAR_TOUCH_GESTURE_H

#include <stdint.h>

#define TG_RELEASE_MS 250U
#define TG_MIN_TAP_MS  30U
#define TG_LONG_MS    800U
#define TG_CAL_MS    4000U

enum TouchEvent : uint8_t {
    TG_NONE = 0,
    TG_WAKE,
    TG_PRESS,
    TG_SHORT_TAP,
    TG_LONG_HOLD,
    TG_CAL_NOTICE,
    TG_CAL_START,
};

struct TouchGestureState {
    bool     downPrev    = false;
    bool     longFired   = false;
    bool     calNotice   = false;
    uint32_t pressStart  = 0;
    uint32_t lastContact = 0;
};

// XPT2046::touched() gates on Z pressure, which dips during a static hold;
// a gap shorter than TG_RELEASE_MS while already pressed still counts as down.
inline bool touchDebouncedDown(bool rawContact, bool downPrev, uint32_t now,
                               uint32_t& lastContact)
{
    if (rawContact) {
        lastContact = now;
        return true;
    }
    return downPrev && (uint32_t)(now - lastContact) < TG_RELEASE_MS;
}

inline TouchEvent touchGestureStep(TouchGestureState& st, bool rawContact,
                                   bool screenOn, uint32_t now)
{
    bool down = touchDebouncedDown(rawContact, st.downPrev, now,
                                   st.lastContact);
    if (down) {
        if (!st.downPrev) {
            st.downPrev   = true;
            st.pressStart = now;
            st.longFired  = false;
            st.calNotice  = false;
            if (!screenOn) {
                st.longFired = true;   // wake consumes the press
                return TG_WAKE;
            }
            return TG_PRESS;
        }
        uint32_t held = now - st.pressStart;
        if (!st.longFired && held >= TG_LONG_MS) {
            st.longFired = true;
            return TG_LONG_HOLD;
        }
        if (held >= TG_CAL_MS) {
            return TG_CAL_START;
        }
        if (!st.calNotice && held >= TG_CAL_MS / 2) {
            st.calNotice = true;
            return TG_CAL_NOTICE;
        }
        return TG_NONE;
    }

    if (!st.downPrev) return TG_NONE;
    st.downPrev  = false;
    st.calNotice = false;
    uint32_t held = now - st.pressStart;
    if (!st.longFired && held >= TG_MIN_TAP_MS) {
        return TG_SHORT_TAP;
    }
    return TG_NONE;
}

#endif // SOFAR_TOUCH_GESTURE_H
