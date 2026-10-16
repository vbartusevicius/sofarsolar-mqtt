#ifndef SOFAR_RESP_CHECK_H
#define SOFAR_RESP_CHECK_H

#include <stdint.h>

inline uint8_t modbusPayloadLen(uint8_t byteCnt, uint8_t frameSize,
                                uint8_t capacity, uint8_t expectedBytes = 0)
{
    if (byteCnt == 0)                  return 0;
    if (byteCnt > capacity)            return 0;   // would overrun the caller
    if (expectedBytes > 0 && byteCnt != expectedBytes) return 0;
    if ((uint16_t)byteCnt + 5 > frameSize) return 0;   // id+fc+cnt+data+crc
    return byteCnt;
}

inline uint8_t modbusExpectedFrame(uint8_t byteCnt, uint8_t maxFrame) {
    uint16_t expected = (uint16_t)byteCnt + 5;
    if (byteCnt == 0 || expected > maxFrame) return 0;
    return (uint8_t)expected;
}

// Minimum silent gap (ms) required before a new Modbus RTU request frame.
// Returns 0 when the bus has already been idle long enough.
inline uint32_t modbusSilenceWaitMs(bool hadActivity, uint32_t now,
                                    uint32_t lastActivityAt,
                                    uint32_t minSilenceMs)
{
    if (!hadActivity) return 0;
    uint32_t elapsed = now - lastActivityAt;
    return elapsed >= minSilenceMs ? 0 : (minSilenceMs - elapsed);
}

#endif // SOFAR_RESP_CHECK_H
