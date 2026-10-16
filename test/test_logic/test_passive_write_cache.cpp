#include <gtest/gtest.h>
#include "inverter/PassiveWriteCache.h"

// These tests guard the inverter flash-wear fix: redundant writes must be
// suppressed, but the keep-alive must still resend before the inverter's
// ~60 s timeout.

TEST(PassiveWriteCache, FirstWriteAlwaysDue) {
    EXPECT_TRUE(passiveWriteDue(false, 0, 0, 0, 1000, 1000, 1000, 45000));
}

TEST(PassiveWriteCache, IdenticalWriteWithinKeepaliveSuppressed) {
    EXPECT_FALSE(passiveWriteDue(true, 1000, 1000, 1000, 1000, 1000, 5000, 45000));
}

TEST(PassiveWriteCache, IdenticalWriteAfterKeepaliveDueAgain) {
    EXPECT_TRUE(passiveWriteDue(true, 1000, 1000, 1000, 1000, 1000, 46000, 45000));
}

TEST(PassiveWriteCache, ChangedValueWritesImmediately) {
    EXPECT_TRUE(passiveWriteDue(true, 1000, 1000, 1000, 2000, 2000, 2000, 45000));
}

TEST(PassiveWriteCache, AsymmetricRangeChangeDetected) {
    // setAuto(-limit, +limit): only one side changing must still trigger
    EXPECT_TRUE(passiveWriteDue(true, -5000, 5000, 1000, -3000, 5000, 2000, 45000));
}

TEST(PassiveWriteCache, MillisWraparound) {
    // lastAt near UINT32_MAX, now just after wrap: 16 ms elapsed
    EXPECT_FALSE(passiveWriteDue(true, 0, 0, UINT32_MAX - 10, 0, 0, 5, 45000));
}

// The HYD's passive timeout (0x1184) is disabled by default, so an unchanged
// command must never be re-sent: 0x1187 may be EEPROM-backed, and a 45 s
// keep-alive is ~1,900 writes/day against a ~100k cycle rating.
TEST(PassiveWriteCache, KeepaliveZeroNeverResends) {
    EXPECT_FALSE(passiveWriteDue(true, 1000, 1000, 0, 1000, 1000, 1, 0));
    EXPECT_FALSE(passiveWriteDue(true, 1000, 1000, 0, 1000, 1000,
                                 3600UL * 24 * 1000, 0));
    EXPECT_FALSE(passiveWriteDue(true, 1000, 1000, 0, 1000, 1000,
                                 UINT32_MAX, 0));
}

// ... but a real change is still written immediately.
TEST(PassiveWriteCache, KeepaliveZeroStillWritesOnChange) {
    EXPECT_TRUE(passiveWriteDue(true, 1000, 1000, 0, 1500, 1000, 1000, 0));
    EXPECT_TRUE(passiveWriteDue(true, 1000, 1000, 0, 1000, 1500, 1000, 0));
}

TEST(PassiveWriteCache, KeepaliveZeroFirstWriteStillHappens) {
    EXPECT_TRUE(passiveWriteDue(false, 0, 0, 0, 0, 0, 0, 0));
}

TEST(SlowPollSchedule, RunsOnFirstCycleRecoveryAndEveryNthCycle) {
    uint8_t tick = 0;
    // First cycle (tick == 0) runs slow tier
    EXPECT_TRUE(inverterSlowPollDue(false, tick, 6));
    // Next 5 cycles skip slow tier
    for (int i = 0; i < 5; i++) {
        EXPECT_FALSE(inverterSlowPollDue(false, tick, 6)) << "i=" << i;
    }
    // 6th cycle runs slow tier again
    EXPECT_TRUE(inverterSlowPollDue(false, tick, 6));
    // Link recovery forces slow tier immediately and resets cadence
    EXPECT_TRUE(inverterSlowPollDue(true, tick, 6));
    EXPECT_FALSE(inverterSlowPollDue(false, tick, 6));
}

TEST(PollStepSequence, FastAndSlowCyclesStepCorrectly) {
    // Fast-only cycle: GRID (0) -> PV (1) -> BATT (2) -> GRID (0)
    EXPECT_EQ(inverterNextPollStep(POLL_GRID, false), POLL_PV);
    EXPECT_EQ(inverterNextPollStep(POLL_PV,   false), POLL_BATT);
    EXPECT_EQ(inverterNextPollStep(POLL_BATT, false), POLL_GRID);

    // Slow cycle: GRID -> PV -> BATT -> SYS -> BATTTOT -> ENERGY -> WORKMODE -> GRID
    EXPECT_EQ(inverterNextPollStep(POLL_BATT,     true), POLL_SYS);
    EXPECT_EQ(inverterNextPollStep(POLL_SYS,      true), POLL_BATTTOT);
    EXPECT_EQ(inverterNextPollStep(POLL_BATTTOT,  true), POLL_ENERGY);
    EXPECT_EQ(inverterNextPollStep(POLL_ENERGY,   true), POLL_WORKMODE);
    EXPECT_EQ(inverterNextPollStep(POLL_WORKMODE, true), POLL_GRID);
}
