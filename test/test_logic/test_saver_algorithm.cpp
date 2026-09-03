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
    EXPECT_TRUE(saverShouldSend(0, -1, 3000, 0, 3000, MIN_DELTA, KEEPALIVE, LAPSE));
}

TEST(SaverShouldSend, SmallDriftSuppressed) {
    // target 1000 → 1050: below 100 W hysteresis, keep-alive not due
    EXPECT_FALSE(saverShouldSend(1050, 1000, 100000, 90000, 0,
                                 MIN_DELTA, KEEPALIVE, LAPSE));
}

TEST(SaverShouldSend, DriftBeyondHysteresisSends) {
    EXPECT_TRUE(saverShouldSend(1150, 1000, 100000, 90000, 0,
                                MIN_DELTA, KEEPALIVE, LAPSE));
}

TEST(SaverShouldSend, NegativeDriftBeyondHysteresisSends) {
    EXPECT_TRUE(saverShouldSend(850, 1000, 100000, 90000, 0,
                                MIN_DELTA, KEEPALIVE, LAPSE));
}

TEST(SaverShouldSend, ZeroCrossingDownAlwaysSends) {
    // any transition to exactly 0 W matters (stops charging)
    EXPECT_TRUE(saverShouldSend(0, 50, 100000, 90000, 100000,
                                MIN_DELTA, KEEPALIVE, LAPSE));
}

TEST(SaverShouldSend, ZeroCrossingUpAlwaysSends) {
    EXPECT_TRUE(saverShouldSend(50, 0, 100000, 90000, 0,
                                MIN_DELTA, KEEPALIVE, LAPSE));
}

TEST(SaverShouldSend, KeepaliveResendsUnchangedTarget) {
    EXPECT_TRUE(saverShouldSend(1000, 1000, 200000, 150000, 0,
                                MIN_DELTA, KEEPALIVE, LAPSE));
}

TEST(SaverShouldSend, IdleLapseStopsAllWritesAtZero) {
    // 10+ min at 0 W: no keep-alive, no writes at all (night-time)
    EXPECT_FALSE(saverShouldSend(0, 0, 800000, 700000, 100000,
                                 MIN_DELTA, KEEPALIVE, LAPSE));
}

TEST(SaverShouldSend, WithinLapseWindowKeepaliveStillRuns) {
    EXPECT_TRUE(saverShouldSend(0, 0, 300000, 200000, 100000,
                                MIN_DELTA, KEEPALIVE, LAPSE));
}

TEST(SaverShouldSend, MillisWraparoundKeepalive) {
    // lastSent just before wrap, now after wrap — 50 s elapsed
    EXPECT_TRUE(saverShouldSend(1000, 1000, 5000, UINT32_MAX - 40000, 0,
                                MIN_DELTA, KEEPALIVE, LAPSE));
}
