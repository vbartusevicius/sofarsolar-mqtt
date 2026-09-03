#ifndef SOFAR_HEAPSTATS_H
#define SOFAR_HEAPSTATS_H

#include <Arduino.h>

struct HeapStats {
    uint32_t freeHeap    = 0;
    uint32_t maxBlock    = 0;
    uint32_t minHeapSeen = UINT32_MAX;
    uint8_t  frag        = 0;

    void update() {
        freeHeap = ESP.getFreeHeap();
        maxBlock = ESP.getMaxFreeBlockSize();
        frag     = ESP.getHeapFragmentation();
        if (freeHeap < minHeapSeen) minHeapSeen = freeHeap;
    }
};
extern HeapStats heapStats;

#endif // SOFAR_HEAPSTATS_H
