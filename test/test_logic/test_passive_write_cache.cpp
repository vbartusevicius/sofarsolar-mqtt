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
