#include <gtest/gtest.h>
#include "display/TouchCal.h"

// Simulates a panel: screen pixel -> raw reading, for a given wiring.
struct Panel {
    bool swap; bool invX; bool invY;
    int16_t xMin = 300, xMax = 3800, yMin = 250, yMax = 3900;

    TouchSample raw(int16_t sx, int16_t sy) const {
        int32_t ax = xMin + (int32_t)(invX ? TCAL_SCREEN_W - 1 - sx : sx)
                          * (xMax - xMin) / (TCAL_SCREEN_W - 1);
        int32_t ay = yMin + (int32_t)(invY ? TCAL_SCREEN_H - 1 - sy : sy)
                          * (yMax - yMin) / (TCAL_SCREEN_H - 1);
        TouchSample s;
        s.x = (int16_t)(swap ? ay : ax);
        s.y = (int16_t)(swap ? ax : ay);
        return s;
    }
};

static TouchCal calibrate(const Panel& p, bool* ok = nullptr) {
    TouchCal c;
    bool r = touchCalBuild(p.raw(TCAL_LO_X, TCAL_LO_Y),
                           p.raw(TCAL_HI_X, TCAL_LO_Y),
                           p.raw(TCAL_LO_X, TCAL_HI_Y), c);
    if (ok) *ok = r;
    return c;
}

// Every wiring permutation must round-trip: calibrate, then map raw back to
// the pixel it came from.
TEST(TouchCal, RoundTripsAllWirings) {
    for (int bits = 0; bits < 8; bits++) {
        Panel p{(bits & 1) != 0, (bits & 2) != 0, (bits & 4) != 0};
        bool ok = false;
        TouchCal c = calibrate(p, &ok);
        ASSERT_TRUE(ok) << "wiring " << bits;
        ASSERT_TRUE(c.valid);
        EXPECT_EQ(c.swap, p.swap) << "wiring " << bits;

        const int16_t pts[][2] = {{24,48},{216,48},{24,288},{216,288},{120,160}};
        for (auto& pt : pts) {
            TouchSample s = p.raw(pt[0], pt[1]);
            int16_t sx = -1, sy = -1;
            touchCalApply(c, s.x, s.y, sx, sy);
            EXPECT_NEAR(sx, pt[0], 2) << "wiring " << bits;
            EXPECT_NEAR(sy, pt[1], 2) << "wiring " << bits;
        }
    }
}

// Corners outside the calibration rectangle must still be reachable.
TEST(TouchCal, ExtrapolatesToScreenEdges) {
    Panel p{true, false, true};
    TouchCal c = calibrate(p);
    TouchSample s = p.raw(0, 0);
    int16_t sx = -1, sy = -1;
    touchCalApply(c, s.x, s.y, sx, sy);
    EXPECT_LE(sx, 3);
    EXPECT_LE(sy, 3);

    s = p.raw(TCAL_SCREEN_W - 1, TCAL_SCREEN_H - 1);
    touchCalApply(c, s.x, s.y, sx, sy);
    EXPECT_GE(sx, TCAL_SCREEN_W - 4);
    EXPECT_GE(sy, TCAL_SCREEN_H - 4);
}

// Results are clamped to the panel, never off-screen.
TEST(TouchCal, ClampsToScreen) {
    Panel p{false, false, false};
    TouchCal c = calibrate(p);
    int16_t sx = -1, sy = -1;
    touchCalApply(c, -5000, -5000, sx, sy);
    EXPECT_EQ(sx, 0);
    EXPECT_EQ(sy, 0);
    touchCalApply(c, 9000, 9000, sx, sy);
    EXPECT_EQ(sx, TCAL_SCREEN_W - 1);
    EXPECT_EQ(sy, TCAL_SCREEN_H - 1);
}

// Tapping the same spot three times cannot yield a calibration.
TEST(TouchCal, RejectsIdenticalSamples) {
    TouchCal c;
    TouchSample s{2000, 2000};
    EXPECT_FALSE(touchCalBuild(s, s, s, c));
    EXPECT_FALSE(c.valid);
}

// Saturated / stuck reads (the failure mode that looked like a bad mapping)
// must be rejected rather than producing a plausible-looking calibration.
TEST(TouchCal, RejectsSaturatedReads) {
    TouchCal c;
    EXPECT_FALSE(touchCalBuild({4095, 4095}, {4095, 4095}, {4095, 4095}, c));
    EXPECT_FALSE(touchCalBuild({0, 0}, {0, 0}, {0, 0}, c));
}

// One dead channel: X travels fine, Y never moves.
TEST(TouchCal, RejectsDeadAxis) {
    TouchCal c;
    EXPECT_FALSE(touchCalBuild({300, 2000}, {3800, 2000}, {300, 2000}, c));
}

// Both screen axes tracking the same raw axis is not a usable panel.
TEST(TouchCal, RejectsBothAxesOnSameChannel) {
    TouchCal c;
    EXPECT_FALSE(touchCalBuild({300, 2000}, {3800, 2000}, {3800, 2000}, c));
}

// Slightly noisy taps (a few counts of jitter) still calibrate.
TEST(TouchCal, ToleratesSampleJitter) {
    Panel p{true, true, false};
    TouchSample a = p.raw(TCAL_LO_X, TCAL_LO_Y);
    TouchSample b = p.raw(TCAL_HI_X, TCAL_LO_Y);
    TouchSample d = p.raw(TCAL_LO_X, TCAL_HI_Y);
    a.x += 6; a.y -= 4; b.x -= 5; b.y += 7; d.x += 3; d.y += 5;
    TouchCal c;
    ASSERT_TRUE(touchCalBuild(a, b, d, c));

    TouchSample mid = p.raw(120, 160);
    int16_t sx = -1, sy = -1;
    touchCalApply(c, mid.x, mid.y, sx, sy);
    EXPECT_NEAR(sx, 120, 8);
    EXPECT_NEAR(sy, 160, 8);
}

// A zero denominator must not divide by zero.
TEST(TouchCal, HandlesDegenerateStoredCal) {
    TouchCal c;
    c.valid = true; c.xLo = c.xHi = 1000; c.yLo = 0; c.yHi = 3000;
    int16_t sx = 5, sy = 5;
    touchCalApply(c, 1500, 1500, sx, sy);
    EXPECT_EQ(sx, -1);
    EXPECT_EQ(sy, -1);
}
