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

#endif // SOFAR_PASSIVE_WRITE_CACHE_H
