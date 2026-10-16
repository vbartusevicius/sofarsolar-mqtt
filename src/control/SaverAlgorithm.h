#ifndef SOFAR_SAVER_ALGORITHM_H
#define SOFAR_SAVER_ALGORITHM_H

#include <stdint.h>

// Pure decision logic for BatterySaver (unit-tested on the native target)

inline int32_t saverAccumulateTarget(int32_t target, int32_t gridPower,
                                     int32_t maxPower) {
    target += gridPower;
    if (target < 0)        return 0;
    if (target > maxPower) return maxPower;
    return target;
}

// Integrates gridPower at most once per sensor poll (sampleSeq), so calling
// update() faster than the sensor poll never double-counts the same reading.
inline int32_t saverIntegrateSample(int32_t target, int32_t gridPower,
                                    int32_t maxPower, uint32_t sampleSeq,
                                    uint32_t& lastSampleSeq)
{
    if (sampleSeq == 0 || sampleSeq == lastSampleSeq) return target;
    lastSampleSeq = sampleSeq;
    return saverAccumulateTarget(target, gridPower, maxPower);
}

#define BSAVE_NATIVE_FAIL_STREAK 10U  // ~30 s of consecutive violation -> fallback

// Closed-loop check for Option A (native [Blo=0, Bup=maxPower] window).
// Returns true if the current sample violates what [0, maxPower] must do:
//   1) battery is discharging (>300 W) despite Blo = 0, or
//   2) grid is exporting (>max(300, minDelta) W) while SOC < 95% and the
//      inverter is not charging (<100 W) despite Bup = maxPower.
inline bool saverNativeWindowViolated(int32_t gridPower, int32_t batteryPower,
                                      uint16_t batterySOC, int32_t maxPower,
                                      int32_t minDelta)
{
    if (batteryPower < -300) return true;
    int32_t exportThreshold = minDelta > 300 ? minDelta : 300;
    if (maxPower >= exportThreshold && gridPower > exportThreshold &&
        batterySOC < 95 && batteryPower < 100) {
        return true;
    }
    return false;
}

// Updates the consecutive-violation streak once per fresh sampleSeq; returns
// true when the streak reaches BSAVE_NATIVE_FAIL_STREAK and Option A should
// fall back to the Option B software loop.
inline bool saverNativeShouldFallback(int32_t gridPower, int32_t batteryPower,
                                      uint16_t batterySOC, int32_t maxPower,
                                      int32_t minDelta, uint32_t sampleSeq,
                                      uint32_t& lastSampleSeq, uint8_t& streak)
{
    if (sampleSeq == 0 || sampleSeq == lastSampleSeq) return false;
    lastSampleSeq = sampleSeq;
    if (saverNativeWindowViolated(gridPower, batteryPower, batterySOC,
                                  maxPower, minDelta)) {
        if (streak < 255) streak++;
    } else {
        streak = 0;
    }
    return streak >= BSAVE_NATIVE_FAIL_STREAK;
}

// Maps a static control mode ("charge", "standby", "auto") to its passive
// power range [outMin, outMax]. Returns false for dynamic/unknown modes.
inline bool modeDesiredPassiveRange(const char* mode, int32_t chargePower,
                                    int32_t autoLimit,
                                    int32_t& outMin, int32_t& outMax)
{
    if (!mode) return false;
    if (mode[0] == 'c' && mode[1] == 'h') {   // "charge"
        outMin = outMax = chargePower;
        return true;
    }
    if (mode[0] == 's' && mode[1] == 't') {   // "standby"
        outMin = outMax = 0;
        return true;
    }
    if (mode[0] == 'a' && mode[1] == 'u') {   // "auto"
        int32_t lim = autoLimit > 0 ? autoLimit : 16384;
        outMin = -lim;
        outMax = lim;
        return true;
    }
    return false;
}

// Write the target only when: changed by ≥ minDelta, crossing 0, or the
// keep-alive is due.  Never write during a sustained 0 W idle (night).
// zeroTiming is an explicit flag rather than "zeroStart != 0": millis()
// legitimately returns 0 at boot and again every 49.7 days, so a timestamp
// cannot double as its own "unset" marker.
inline bool saverShouldSend(int32_t target, int32_t lastSentTarget,
                            uint32_t now, uint32_t lastSentAt,
                            bool zeroTiming, uint32_t zeroStart,
                            int32_t minDelta, uint32_t keepaliveMs,
                            uint32_t idleLapseMs)
{
    if (target == 0 && zeroTiming && (uint32_t)(now - zeroStart) >= idleLapseMs)
        return false;
    int32_t delta = target - lastSentTarget;
    if (delta >= minDelta || delta <= -minDelta) return true;
    if ((target == 0) != (lastSentTarget == 0))  return true;
    if (keepaliveMs == 0)                        return false;   // no timeout
    return (uint32_t)(now - lastSentAt) >= keepaliveMs;
}

#endif // SOFAR_SAVER_ALGORITHM_H
