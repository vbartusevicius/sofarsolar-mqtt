#include <gtest/gtest.h>
#include "control/SaverAlgorithm.h"

static constexpr int32_t MIN_DELTA = 100;
static constexpr uint32_t KEEPALIVE = 45000;
static constexpr uint32_t LAPSE = 600000;

TEST(SaverAccumulate, ClampsToZero) {
    EXPECT_EQ(saverAccumulateTarget(500, -800, 20000), 0);
}

TEST(SaverAccumulate, ClampsToMax) {
    EXPECT_EQ(saverAccumulateTarget(19900, 500, 20000), 20000);
}

TEST(SaverAccumulate, AccumulatesExport) {
    EXPECT_EQ(saverAccumulateTarget(1000, 250, 20000), 1250);
}

TEST(SaverShouldSend, FirstCycleAfterEnable) {
    // lastSentTarget = -1 sentinel: zero crossing (0 vs -1) must send 0 W
    EXPECT_TRUE(saverShouldSend(0, -1, 3000, 0, true, 3000, MIN_DELTA, KEEPALIVE, LAPSE));
}

TEST(SaverShouldSend, SmallDriftSuppressed) {
    // target 1000 → 1050: below 100 W hysteresis, keep-alive not due
    EXPECT_FALSE(saverShouldSend(1050, 1000, 100000, 90000, false, 0, MIN_DELTA, KEEPALIVE, LAPSE));
}

TEST(SaverShouldSend, DriftBeyondHysteresisSends) {
    EXPECT_TRUE(saverShouldSend(1150, 1000, 100000, 90000, false, 0, MIN_DELTA, KEEPALIVE, LAPSE));
}

TEST(SaverShouldSend, NegativeDriftBeyondHysteresisSends) {
    EXPECT_TRUE(saverShouldSend(850, 1000, 100000, 90000, false, 0, MIN_DELTA, KEEPALIVE, LAPSE));
}

TEST(SaverShouldSend, ZeroCrossingDownAlwaysSends) {
    // any transition to exactly 0 W matters (stops charging)
    EXPECT_TRUE(saverShouldSend(0, 50, 100000, 90000, true, 100000, MIN_DELTA, KEEPALIVE, LAPSE));
}

TEST(SaverShouldSend, ZeroCrossingUpAlwaysSends) {
    EXPECT_TRUE(saverShouldSend(50, 0, 100000, 90000, false, 0, MIN_DELTA, KEEPALIVE, LAPSE));
}

TEST(SaverShouldSend, KeepaliveResendsUnchangedTarget) {
    EXPECT_TRUE(saverShouldSend(1000, 1000, 200000, 150000, false, 0, MIN_DELTA, KEEPALIVE, LAPSE));
}

TEST(SaverShouldSend, IdleLapseStopsAllWritesAtZero) {
    // 10+ min at 0 W: no keep-alive, no writes at all (night-time)
    EXPECT_FALSE(saverShouldSend(0, 0, 800000, 700000, true, 100000, MIN_DELTA, KEEPALIVE, LAPSE));
}

TEST(SaverShouldSend, WithinLapseWindowKeepaliveStillRuns) {
    EXPECT_TRUE(saverShouldSend(0, 0, 300000, 200000, true, 100000, MIN_DELTA, KEEPALIVE, LAPSE));
}

TEST(SaverShouldSend, MillisWraparoundKeepalive) {
    // lastSent just before wrap, now after wrap — 50 s elapsed
    EXPECT_TRUE(saverShouldSend(1000, 1000, 5000, UINT32_MAX - 40000, false, 0, MIN_DELTA, KEEPALIVE, LAPSE));
}

// millis() returns 0 at boot and again every 49.7 days. A zeroStart of 0 with
// timing active must still suppress writes, which the old "zeroStart != 0"
// sentinel could not express.
TEST(SaverShouldSend, IdleLapseWorksWhenZeroStartIsZero) {
    EXPECT_FALSE(saverShouldSend(0, 0, LAPSE + 1000, LAPSE, true, 0,
                                 MIN_DELTA, KEEPALIVE, LAPSE));
}

// Same instant, but nothing is being timed: keep-alive must still run.
TEST(SaverShouldSend, ZeroStartZeroWithoutTimingStillKeepalives) {
    const uint32_t now = LAPSE + 1000;
    EXPECT_TRUE(saverShouldSend(0, 0, now, now - KEEPALIVE - 1, false, 0,
                                MIN_DELTA, KEEPALIVE, LAPSE));
}

// Idle window spanning the wraparound must not restart the lapse timer.
TEST(SaverShouldSend, IdleLapseSpansMillisWraparound) {
    EXPECT_FALSE(saverShouldSend(0, 0, 5000, UINT32_MAX - 600000, true,
                                 UINT32_MAX - 600000,
                                 MIN_DELTA, KEEPALIVE, LAPSE));
}

// Keep-alive disabled (inverter passive timeout == 0): a steady target must
// produce no writes at all, however long it runs.
TEST(SaverShouldSend, KeepaliveZeroSuppressesPeriodicResend) {
    EXPECT_FALSE(saverShouldSend(1000, 1000, 3600UL * 1000, 0, false, 0,
                                 MIN_DELTA, 0, LAPSE));
    EXPECT_FALSE(saverShouldSend(1000, 1000, UINT32_MAX, 0, false, 0,
                                 MIN_DELTA, 0, LAPSE));
}

// Real changes and zero crossings are still written with keep-alive disabled.
TEST(SaverShouldSend, KeepaliveZeroStillSendsOnChange) {
    EXPECT_TRUE(saverShouldSend(1200, 1000, 100000, 0, false, 0,
                                MIN_DELTA, 0, LAPSE));
    EXPECT_TRUE(saverShouldSend(0, 1000, 100000, 0, false, 0,
                                MIN_DELTA, 0, LAPSE));
}

// A single sensor reading (same sampleSeq) must never be integrated twice when
// BatterySaver::update() runs between sensor polls.
TEST(SaverIntegrateSample, IntegratesEachSampleAtMostOnce) {
    uint32_t lastSeq = 0;
    int32_t target = 1000;
    // Sample #1 (+300 W export) arrives
    target = saverIntegrateSample(target, 300, 20000, 1, lastSeq);
    EXPECT_EQ(target, 1300);
    EXPECT_EQ(lastSeq, 1u);
    // update() called again before next sensor read -> no double count
    target = saverIntegrateSample(target, 300, 20000, 1, lastSeq);
    EXPECT_EQ(target, 1300);
    // Sample #2 (+200 W export) arrives -> integrated once
    target = saverIntegrateSample(target, 200, 20000, 2, lastSeq);
    EXPECT_EQ(target, 1500);
    EXPECT_EQ(lastSeq, 2u);
}

TEST(SaverIntegrateSample, IgnoresUninitializedSampleZero) {
    uint32_t lastSeq = 0;
    EXPECT_EQ(saverIntegrateSample(500, 400, 20000, 0, lastSeq), 500);
    EXPECT_EQ(lastSeq, 0u);
}

TEST(NativeWindowCheck, DetectsUncommandedDischargeAndIgnoredSurplus) {
    // Healthy: grid near 0, battery charging 1500 W at 60% SOC
    EXPECT_FALSE(saverNativeWindowViolated(40, 1500, 60, 20000, 100));
    // Violation 1: discharging > 300 W despite Blo = 0
    EXPECT_TRUE(saverNativeWindowViolated(0, -450, 60, 20000, 100));
    // Violation 2: exporting 600 W while SOC < 95% and battery not charging
    EXPECT_TRUE(saverNativeWindowViolated(600, 0, 60, 20000, 100));
    // Not a violation when battery is already nearly full (SOC >= 95%)
    EXPECT_FALSE(saverNativeWindowViolated(600, 0, 96, 20000, 100));
}

TEST(NativeWindowCheck, FallbackRequiresConsecutiveSampleViolations) {
    uint32_t lastSeq = 0;
    uint8_t streak = 0;
    for (uint32_t seq = 1; seq < BSAVE_NATIVE_FAIL_STREAK; seq++) {
        EXPECT_FALSE(saverNativeShouldFallback(600, 0, 60, 20000, 100,
                                               seq, lastSeq, streak));
    }
    // Same sampleSeq repeated must not advance streak
    EXPECT_FALSE(saverNativeShouldFallback(600, 0, 60, 20000, 100,
                                           BSAVE_NATIVE_FAIL_STREAK - 1,
                                           lastSeq, streak));
    // 6th fresh violating sample triggers fallback to Option B
    EXPECT_TRUE(saverNativeShouldFallback(600, 0, 60, 20000, 100,
                                          BSAVE_NATIVE_FAIL_STREAK,
                                          lastSeq, streak));
}
