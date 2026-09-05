#ifndef SOFAR_RESP_CHECK_H
#define SOFAR_RESP_CHECK_H

#include <stdint.h>

inline uint8_t modbusPayloadLen(uint8_t byteCnt, uint8_t frameSize,
                                uint8_t capacity)
{
    if (byteCnt == 0)                  return 0;
    if (byteCnt > capacity)            return 0;   // would overrun the caller
    if ((uint16_t)byteCnt + 5 > frameSize) return 0;   // id+fc+cnt+data+crc
    return byteCnt;
}

inline uint8_t modbusExpectedFrame(uint8_t byteCnt, uint8_t maxFrame) {
    uint16_t expected = (uint16_t)byteCnt + 5;
    if (byteCnt == 0 || expected > maxFrame) return 0;
    return (uint8_t)expected;
}

#endif // SOFAR_RESP_CHECK_H
