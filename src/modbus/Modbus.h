#ifndef SOFAR_MODBUS_H
#define SOFAR_MODBUS_H

#include <Arduino.h>
#include "Config.h"

class Modbus {
public:
    void begin(unsigned long baud);
    // dataCap: capacity of data[]; responses claiming more are rejected
    bool readHolding(uint8_t slaveId, uint16_t reg, uint8_t count,
                     uint8_t* data, uint8_t dataCap, uint8_t& dataSize);
    bool writeMultiple(uint8_t slaveId, uint16_t reg, uint8_t regCount,
                       const uint8_t* payload, uint8_t payloadLen);

private:
    static constexpr uint8_t MAX_RESP = MODBUS_MAX_FRAME;

    void flush();
    int  listen(uint8_t slaveId, uint8_t* frame, uint8_t& frameSize,
                uint8_t* data, uint8_t dataCap, uint8_t& dataSize);
    void calcCRC(uint8_t* frame, uint8_t len);
    bool checkCRC(const uint8_t* frame, uint8_t len) const;
};

#endif
