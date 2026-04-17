#ifndef SOFAR_MODBUS_H
#define SOFAR_MODBUS_H

#include <Arduino.h>
#include "Config.h"

class Modbus {
public:
    void begin(unsigned long baud);

    // Read `count` holding registers starting at `reg`.
    // On success fills `data` (2 bytes per register) and sets `dataSize`.
    bool readHolding(uint8_t slaveId, uint16_t reg, uint8_t count,
                     uint8_t* data, uint8_t& dataSize);

    // Write `regCount` registers (FC 0x10).
    // `payload` is the raw byte content (2 bytes per register).
    bool writeMultiple(uint8_t slaveId, uint16_t reg, uint8_t regCount,
                       const uint8_t* payload, uint8_t payloadLen);

private:
    static constexpr uint8_t MAX_RESP = MODBUS_MAX_FRAME;

    void flush();
    int  listen(uint8_t* frame, uint8_t& frameSize,
                uint8_t* data, uint8_t& dataSize);
    void calcCRC(uint8_t* frame, uint8_t len);
    bool checkCRC(uint8_t* frame, uint8_t len);
};

#endif // SOFAR_MODBUS_H
