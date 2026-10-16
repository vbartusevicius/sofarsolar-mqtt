#include <gtest/gtest.h>
#include "display/TouchGesture.h"

TEST(TouchGesture, ShortTapFiresOnRelease) {
    TouchGestureState st;
    EXPECT_EQ(touchGestureStep(st, true, true, 1000), TG_PRESS);
    EXPECT_EQ(touchGestureStep(st, true, true, 1100), TG_NONE);
    // Contact drops; after TG_RELEASE_MS debounce expires, release fires tap
    EXPECT_EQ(touchGestureStep(st, false, true, 1200), TG_NONE);
    EXPECT_EQ(touchGestureStep(st, false, true, 1360), TG_SHORT_TAP);
}

TEST(TouchGesture, WakeConsumesEntirePress) {
    TouchGestureState st;
    EXPECT_EQ(touchGestureStep(st, true, false, 1000), TG_WAKE);
    // Even if held past TG_LONG_MS or released quickly, no tap/tab-switch fires
    EXPECT_EQ(touchGestureStep(st, true, true, 1900), TG_NONE);
    EXPECT_EQ(touchGestureStep(st, false, true, 2200), TG_NONE);
}

TEST(TouchGesture, PressureDipDuringHoldDoesNotReset4sCalibrationTimer) {
    TouchGestureState st;
    EXPECT_EQ(touchGestureStep(st, true, true, 1000), TG_PRESS);
    EXPECT_EQ(touchGestureStep(st, true, true, 1850), TG_LONG_HOLD);
    EXPECT_EQ(touchGestureStep(st, true, true, 2400), TG_NONE);
    // Brief 100 ms pressure dip at t=2500 (< 250 ms since t=2400)
    EXPECT_EQ(touchGestureStep(st, false, true, 2500), TG_NONE);
    // Contact returns at t=2600 -> hold continues, notice fires at >=2000ms held
    EXPECT_EQ(touchGestureStep(st, true, true, 3050), TG_CAL_NOTICE);
    // Reaches 4000 ms held -> starts calibration
    EXPECT_EQ(touchGestureStep(st, true, true, 5000), TG_CAL_START);
}

TEST(TouchGesture, HoldSpansMillisWraparound) {
    TouchGestureState st;
    uint32_t t0 = UINT32_MAX - 300;
    EXPECT_EQ(touchGestureStep(st, true, true, t0), TG_PRESS);
    EXPECT_EQ(touchGestureStep(st, true, true, t0 + 850), TG_LONG_HOLD);
    EXPECT_EQ(touchGestureStep(st, true, true, t0 + 4000), TG_CAL_START);
}
