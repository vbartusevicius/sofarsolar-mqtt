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
