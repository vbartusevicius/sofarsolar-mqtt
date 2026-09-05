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
