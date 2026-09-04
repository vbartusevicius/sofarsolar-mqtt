#ifndef SOFAR_PASSIVE_WRITE_CACHE_H
#define SOFAR_PASSIVE_WRITE_CACHE_H

#include <stdint.h>

// REG_PASSIVE_CTRL lives in the inverter's NVM: identical writes are
// re-sent at most every keepaliveMs.
inline bool passiveWriteDue(bool cacheValid, int32_t lastMin, int32_t lastMax,
                            uint32_t lastAt, int32_t minP, int32_t maxP,
                            uint32_t now, uint32_t keepaliveMs)
{
    if (!cacheValid)                          return true;
    if (minP != lastMin || maxP != lastMax)   return true;
    return (uint32_t)(now - lastAt) >= keepaliveMs;
}

#endif // SOFAR_PASSIVE_WRITE_CACHE_H
