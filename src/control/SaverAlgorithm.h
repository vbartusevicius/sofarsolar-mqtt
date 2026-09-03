#ifndef SOFAR_SAVER_ALGORITHM_H
#define SOFAR_SAVER_ALGORITHM_H

#include <stdint.h>

// Pure decision logic for BatterySaver

// Accumulate grid power into the charge target and clamp it so the
// battery never discharges to the grid.
inline int32_t saverAccumulateTarget(int32_t target, int32_t gridPower,
                                     int32_t maxPower) {
    target += gridPower;
    if (target < 0)        return 0;
    if (target > maxPower) return maxPower;
    return target;
}

// Decide whether the (already clamped) target must be written to the
// inverter now.  Mirror of BatterySaver::update() semantics:
//  · after idleLapseMs at 0 W (night) never write
//  · write when the target changed by at least minDelta (hysteresis)
//  · write on any 0 ↔ non-0 crossing
//  · otherwise re-write only as keep-alive (inverter times out ~60 s)
inline bool saverShouldSend(int32_t target, int32_t lastSentTarget,
                            uint32_t now, uint32_t lastSentAt,
                            uint32_t zeroStart,
                            int32_t minDelta, uint32_t keepaliveMs,
                            uint32_t idleLapseMs)
{
    if (target == 0 && zeroStart != 0 && (uint32_t)(now - zeroStart) >= idleLapseMs)
        return false;
    int32_t delta = target - lastSentTarget;
    if (delta >= minDelta || delta <= -minDelta) return true;
    if ((target == 0) != (lastSentTarget == 0))  return true;
    return (uint32_t)(now - lastSentAt) >= keepaliveMs;
}

#endif // SOFAR_SAVER_ALGORITHM_H
