#ifndef SOFAR_CRC16_H
#define SOFAR_CRC16_H

#include <stdint.h>

inline uint16_t modbusCrc16(const uint8_t* data, uint8_t len) {
    uint16_t crc = 0xFFFF;
    for (uint8_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 1) { crc >>= 1; crc ^= 0xA001; }
            else          { crc >>= 1; }
        }
    }
    return crc;
}

#endif // SOFAR_CRC16_H
