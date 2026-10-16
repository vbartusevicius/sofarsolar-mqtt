#ifndef SOFAR_PASSIVE_WRITE_CACHE_H
#define SOFAR_PASSIVE_WRITE_CACHE_H

#include <stdint.h>

// Identical writes are re-sent at most every keepaliveMs; keepaliveMs == 0
// means never — the HYD's passive timeout is disabled by default, so a
// re-send would be a pointless write to a register that may be EEPROM-backed.
inline bool passiveWriteDue(bool cacheValid, int32_t lastMin, int32_t lastMax,
                            uint32_t lastAt, int32_t minP, int32_t maxP,
                            uint32_t now, uint32_t keepaliveMs)
{
    if (!cacheValid)                          return true;
    if (minP != lastMin || maxP != lastMax)   return true;
    if (keepaliveMs == 0)                     return false;
    return (uint32_t)(now - lastAt) >= keepaliveMs;
}

// Slow registers (temps, lifetime energy, working mode) are read on link
// recovery and every slowDiv fast-poll cycles, cutting bus traffic in half.
inline bool inverterSlowPollDue(bool recovering, uint8_t& slowTick,
                                uint8_t slowDiv)
{
    if (recovering || slowDiv <= 1) {
        slowTick = 1;
        return true;
    }
    if (slowTick == 0 || slowTick >= slowDiv) {
        slowTick = 1;
        return true;
    }
    slowTick++;
    return false;
}

enum InverterPollStep : uint8_t {
    POLL_GRID = 0,
    POLL_PV,
    POLL_BATT,
    POLL_SYS,
    POLL_BATTTOT,
    POLL_ENERGY,
    POLL_WORKMODE,
};

inline uint8_t inverterNextPollStep(uint8_t step, bool slowCycleActive) {
    if (step < POLL_BATT)     return step + 1;
    if (step == POLL_BATT)    return slowCycleActive ? POLL_SYS : POLL_GRID;
    if (step < POLL_WORKMODE) return step + 1;
    return POLL_GRID;
}

#endif // SOFAR_PASSIVE_WRITE_CACHE_H
