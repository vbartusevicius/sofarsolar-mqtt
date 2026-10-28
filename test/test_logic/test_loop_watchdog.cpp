#include <gtest/gtest.h>
#include "util/LoopWatchdog.h"

TEST(LoopWatchdog, HealthyLoopIsNotStalled) {
    EXPECT_FALSE(loopStalled(1000, 990, LOOP_STALL_MS));
    EXPECT_FALSE(loopStalled(LOOP_STALL_MS - 1, 0, LOOP_STALL_MS));
}

TEST(LoopWatchdog, StallDetectedAtLimit) {
    EXPECT_TRUE(loopStalled(LOOP_STALL_MS, 0, LOOP_STALL_MS));
    EXPECT_TRUE(loopStalled(5000 + LOOP_STALL_MS * 3, 5000, LOOP_STALL_MS));
}

TEST(LoopWatchdog, MillisWrapIsHandled) {
    uint32_t beat = 0xFFFFFF00UL;
    EXPECT_FALSE(loopStalled(beat + 1000, beat, LOOP_STALL_MS));
    EXPECT_TRUE(loopStalled(beat + LOOP_STALL_MS, beat, LOOP_STALL_MS));
}

TEST(LoopWatchdog, SealedRecordValidates) {
    LoopWdRecord r;
    loopWdSeal(r, STAGE_MQTT_PUB, true, 604800);
    EXPECT_TRUE(loopWdValid(r));
    EXPECT_EQ(r.stage, STAGE_MQTT_PUB);
    EXPECT_EQ(r.stalled, 1);
    EXPECT_EQ(r.uptimeS, 604800u);
}

TEST(LoopWatchdog, GarbageRtcMemoryIsRejected) {
    LoopWdRecord r;
    loopWdSeal(r, STAGE_WEB, false, 42);
    r.uptimeS ^= 1;                       // bit flip
    EXPECT_FALSE(loopWdValid(r));

    LoopWdRecord junk = {0xDEADBEEF, 3, 0, 0, 7, 0};
    EXPECT_FALSE(loopWdValid(junk));

    loopWdSeal(r, STAGE_COUNT, false, 1);  // out-of-range stage
    EXPECT_FALSE(loopWdValid(r));
}

TEST(LoopWatchdog, EveryStageHasAName) {
    for (uint8_t s = 0; s < STAGE_COUNT; s++)
        EXPECT_STRNE(loopStageName(s), "unknown");
    EXPECT_STREQ(loopStageName(STAGE_COUNT), "unknown");
}
