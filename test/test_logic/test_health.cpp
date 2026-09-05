#include <gtest/gtest.h>
#include "util/Health.h"

static const uint32_t OK_HEAP  = 20000;
static const uint32_t OK_BLOCK = 12000;

TEST(Health, HealthyDeviceDoesNothing) {
    HealthState st;
    for (uint32_t t = 0; t < 600000; t += 60000)
        EXPECT_EQ(healthEvaluate(st, true, t, OK_HEAP, OK_BLOCK), HEALTH_OK);
}

TEST(Health, ShortWifiOutageIsTolerated) {
    HealthState st;
    EXPECT_EQ(healthEvaluate(st, false, 60000, OK_HEAP, OK_BLOCK), HEALTH_OK);
    EXPECT_EQ(healthEvaluate(st, false, 90000, OK_HEAP, OK_BLOCK), HEALTH_OK);
    EXPECT_EQ(healthEvaluate(st, true, 120000, OK_HEAP, OK_BLOCK), HEALTH_OK);
}

TEST(Health, WifiDownPastRetryWindowAsksForReconnect) {
    HealthState st;
    healthEvaluate(st, false, 0, OK_HEAP, OK_BLOCK);
    EXPECT_EQ(healthEvaluate(st, false, HEALTH_WIFI_RETRY_MS, OK_HEAP, OK_BLOCK),
              HEALTH_WIFI_RETRY);
}

TEST(Health, RetryIsRateLimited) {
    HealthState st;
    healthEvaluate(st, false, 0, OK_HEAP, OK_BLOCK);
    EXPECT_EQ(healthEvaluate(st, false, HEALTH_WIFI_RETRY_MS, OK_HEAP, OK_BLOCK),
              HEALTH_WIFI_RETRY);
    EXPECT_EQ(healthEvaluate(st, false, HEALTH_WIFI_RETRY_MS + 60000,
                             OK_HEAP, OK_BLOCK), HEALTH_OK);
    EXPECT_EQ(healthEvaluate(st, false, HEALTH_WIFI_RETRY_MS * 2 + 1,
                             OK_HEAP, OK_BLOCK), HEALTH_WIFI_RETRY);
}

TEST(Health, WifiDownLongEnoughReboots) {
    HealthState st;
    healthEvaluate(st, false, 1000, OK_HEAP, OK_BLOCK);
    EXPECT_EQ(healthEvaluate(st, false, 1000 + HEALTH_WIFI_REBOOT_MS,
                             OK_HEAP, OK_BLOCK), HEALTH_REBOOT_WIFI);
}

// Reconnecting resets the timer, so an outage every 25 min never reboots.
TEST(Health, ReconnectResetsTheDownTimer) {
    HealthState st;
    healthEvaluate(st, false, 0, OK_HEAP, OK_BLOCK);
    healthEvaluate(st, false, HEALTH_WIFI_REBOOT_MS - 1000, OK_HEAP, OK_BLOCK);
    healthEvaluate(st, true, HEALTH_WIFI_REBOOT_MS, OK_HEAP, OK_BLOCK);
    healthEvaluate(st, false, HEALTH_WIFI_REBOOT_MS + 1000, OK_HEAP, OK_BLOCK);
    // retries resume, but the reboot deadline restarted from the new outage
    EXPECT_EQ(healthEvaluate(st, false, HEALTH_WIFI_REBOOT_MS * 2 - 1000,
                             OK_HEAP, OK_BLOCK), HEALTH_WIFI_RETRY);
}

// A WiFi outage spanning the 49.7-day wraparound must still be measured
// correctly rather than resetting or rebooting immediately.
TEST(Health, WifiOutageSpansMillisWraparound) {
    HealthState st;
    uint32_t before = UINT32_MAX - 60000;
    healthEvaluate(st, false, before, OK_HEAP, OK_BLOCK);
    EXPECT_EQ(healthEvaluate(st, false, 30000, OK_HEAP, OK_BLOCK), HEALTH_OK);
    EXPECT_EQ(healthEvaluate(st, false, 61000, OK_HEAP, OK_BLOCK),
              HEALTH_WIFI_RETRY);
    EXPECT_EQ(healthEvaluate(st, false, HEALTH_WIFI_REBOOT_MS, OK_HEAP, OK_BLOCK),
              HEALTH_REBOOT_WIFI);
}

// One low sample is not enough: a transient dip during a web upload or an MQTT
// reconnect is normal.
TEST(Health, SingleLowHeapSampleIsIgnored) {
    HealthState st;
    EXPECT_EQ(healthEvaluate(st, true, 1000, 3000, OK_BLOCK), HEALTH_OK);
    EXPECT_EQ(healthEvaluate(st, true, 2000, OK_HEAP, OK_BLOCK), HEALTH_OK);
    EXPECT_EQ(st.lowStreak, 0);
}

TEST(Health, SustainedLowHeapReboots) {
    HealthState st;
    HealthAction a = HEALTH_OK;
    for (int i = 0; i < HEALTH_LOW_STREAK; i++)
        a = healthEvaluate(st, true, 1000 * i, 3000, OK_BLOCK);
    EXPECT_EQ(a, HEALTH_REBOOT_HEAP);
}

// Fragmentation alone: plenty free, but no block big enough for MQTT's buffer.
TEST(Health, SustainedFragmentationReboots) {
    HealthState st;
    HealthAction a = HEALTH_OK;
    for (int i = 0; i < HEALTH_LOW_STREAK; i++)
        a = healthEvaluate(st, true, 1000 * i, 20000, 1500);
    EXPECT_EQ(a, HEALTH_REBOOT_HEAP);
}

// Heap trouble takes priority over WiFi state.
TEST(Health, HeapRebootWinsOverWifiState) {
    HealthState st;
    HealthAction a = HEALTH_OK;
    for (int i = 0; i < HEALTH_LOW_STREAK; i++)
        a = healthEvaluate(st, false, 1000 * i, 1000, 500);
    EXPECT_EQ(a, HEALTH_REBOOT_HEAP);
}

// Recovery clears the streak, so an intermittently busy device never reboots.
TEST(Health, AlternatingLowAndOkNeverReboots) {
    HealthState st;
    for (int i = 0; i < 50; i++) {
        uint32_t heap = (i % 2) ? 3000 : OK_HEAP;
        EXPECT_EQ(healthEvaluate(st, true, 1000 * i, heap, OK_BLOCK), HEALTH_OK);
    }
}
