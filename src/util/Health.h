#ifndef SOFAR_HEALTH_H
#define SOFAR_HEALTH_H

#include <stdint.h>

#define HEALTH_WIFI_RETRY_MS   120000UL   // nudge a reconnect after 2 min down
#define HEALTH_WIFI_REBOOT_MS 1800000UL   // reboot after 30 min down
#define HEALTH_HEAP_MIN           6000U   // bytes of free heap
#define HEALTH_BLOCK_MIN          4000U   // largest allocatable block
#define HEALTH_LOW_STREAK             5   // consecutive low samples

enum HealthAction : uint8_t {
    HEALTH_OK = 0,
    HEALTH_WIFI_RETRY,
    HEALTH_REBOOT_WIFI,
    HEALTH_REBOOT_HEAP,
};

struct HealthState {
    bool     wifiDown      = false;
    uint32_t wifiDownSince = 0;
    uint32_t lastRetryAt   = 0;
    bool     retried       = false;
    uint8_t  lowStreak     = 0;
};

inline HealthAction healthEvaluate(HealthState& st, bool wifiOk, uint32_t now,
                                   uint32_t freeHeap, uint32_t maxBlock)
{
    if (freeHeap < HEALTH_HEAP_MIN || maxBlock < HEALTH_BLOCK_MIN) {
        if (st.lowStreak < 255) st.lowStreak++;
    } else {
        st.lowStreak = 0;
    }
    if (st.lowStreak >= HEALTH_LOW_STREAK) return HEALTH_REBOOT_HEAP;

    if (wifiOk) {
        st.wifiDown = false;
        st.retried  = false;
        return HEALTH_OK;
    }

    if (!st.wifiDown) {
        st.wifiDown      = true;
        st.wifiDownSince = now;
        st.retried       = false;
        return HEALTH_OK;
    }

    uint32_t downFor = now - st.wifiDownSince;
    if (downFor >= HEALTH_WIFI_REBOOT_MS) return HEALTH_REBOOT_WIFI;

    bool retryDue = !st.retried ||
                    (uint32_t)(now - st.lastRetryAt) >= HEALTH_WIFI_RETRY_MS;
    if (downFor >= HEALTH_WIFI_RETRY_MS && retryDue) {
        st.retried     = true;
        st.lastRetryAt = now;
        return HEALTH_WIFI_RETRY;
    }
    return HEALTH_OK;
}

#endif // SOFAR_HEALTH_H
