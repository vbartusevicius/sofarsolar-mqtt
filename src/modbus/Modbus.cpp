#include "Modbus.h"
#include "Crc16.h"
#include "util/AppLog.h"

void Modbus::begin(unsigned long baud) {
    Serial.begin(baud);
}

// ── CRC-16 / Modbus ────────────────────────────────────────────
void Modbus::calcCRC(uint8_t* frame, uint8_t len) {
    uint16_t crc = modbusCrc16(frame, len - 2);
    frame[len - 2] = crc & 0xFF;
    frame[len - 1] = crc >> 8;
}

bool Modbus::checkCRC(const uint8_t* frame, uint8_t len) const {
    // Modbus CRC is little-endian: low byte first
    uint16_t received = frame[len - 2] | ((uint16_t)frame[len - 1] << 8);
    return received == modbusCrc16(frame, len - 2);
}

// ── Flush serial buffers ───────────────────────────────────────
void Modbus::flush() {
    Serial.flush();
    delay(20);
    while (Serial.available()) Serial.read();
}

// ── Listen for a response frame ────────────────────────────────
// The first byte may take up to MODBUS_TIMEOUT_MS to arrive;
// subsequent bytes within the frame must follow within
// MODBUS_INTERBYTE_MS, so a dead link can only block one transfer.
int Modbus::listen(uint8_t slaveId, uint8_t* frame, uint8_t& frameSize,
                   uint8_t* data, uint8_t& dataSize)
{
    uint8_t idx = 0, fnCode = 0, expected = 0;
    dataSize = 0;
    frameSize = 0;

    while (idx < MAX_RESP) {
        unsigned long limit = (idx == 0) ? MODBUS_TIMEOUT_MS : MODBUS_INTERBYTE_MS;
        unsigned long start = millis();
        while (!Serial.available() && (millis() - start) < limit) delay(1);
        if (!Serial.available()) break;

        frame[idx] = Serial.read();

        if (idx == 0 && frame[0] != slaveId) continue;

        if (idx == 1) {
            fnCode = frame[1];
            if (fnCode & 0x80)              expected = 5;  // error response
            else if (fnCode == FC_WRITE_MULTI) expected = 8;  // FC16 echo
        }

        if (idx == 2 && fnCode == FC_READ_HOLDING) {
            expected = 3 + frame[2] + 2;   // id + fc + byteCnt + data + crc
            if (expected > MAX_RESP) return -1;
        }

        idx++;
        if (expected > 0 && idx >= expected) break;
    }

    frameSize = idx;
    if (frameSize < 5) {
        char lb[40];
        snprintf(lb, sizeof(lb), "Resp timeout got %uB", (unsigned)frameSize);
        appLog.add("MB", lb);
        return -2;
    }
    if (!checkCRC(frame, frameSize)) {
        char lb[48];
        snprintf(lb, sizeof(lb), "Resp CRC fail fc=0x%02X len=%u", fnCode, (unsigned)frameSize);
        appLog.add("MB", lb);
        return -1;
    }
    if (fnCode & 0x80) {
        char lb[40];
        snprintf(lb, sizeof(lb), "Resp err fc=0x%02X code=%u", fnCode, frame[2]);
        appLog.add("MB", lb);
        return frame[2];
    }

    // Extract data bytes for FC03 read responses
    if (fnCode == FC_READ_HOLDING && frameSize >= 5) {
        uint8_t byteCnt = frame[2];
        for (uint8_t i = 0; i < byteCnt; i++) {
            data[i] = frame[3 + i];
            dataSize++;
        }
    }

    return 0;
}

// ── Read Holding Registers (FC 0x03) ───────────────────────────
bool Modbus::readHolding(uint8_t slaveId, uint16_t reg, uint8_t count,
                         uint8_t* data, uint8_t& dataSize)
{
    uint8_t frame[8] = {
        slaveId, FC_READ_HOLDING,
        (uint8_t)(reg >> 8), (uint8_t)(reg & 0xFF),
        0, count,
        0, 0
    };
    calcCRC(frame, 8);
    flush();
    Serial.write(frame, 8);

    uint8_t resp[MAX_RESP];
    uint8_t respSize = 0;
    int rc = listen(slaveId, resp, respSize, data, dataSize);
    if (rc != 0) {
        char lb[48];
        snprintf(lb, sizeof(lb), "RD 0x%04X x%u FAIL rc=%d", reg, count, rc);
        appLog.add("MB", lb);
    }
    return rc == 0;
}

// ── Write Multiple Registers (FC 0x10) ─────────────────────────
bool Modbus::writeMultiple(uint8_t slaveId, uint16_t reg, uint8_t regCount,
                           const uint8_t* payload, uint8_t payloadLen)
{
    // Frame: id, fc, regHi, regLo, cntHi, cntLo, byteCount, ...payload, crcLo, crcHi
    uint8_t frameLen = 7 + payloadLen + 2;
    if (frameLen > MAX_RESP) return false;

    uint8_t frame[MAX_RESP];
    frame[0] = slaveId;
    frame[1] = FC_WRITE_MULTI;
    frame[2] = reg >> 8;
    frame[3] = reg & 0xFF;
    frame[4] = 0;
    frame[5] = regCount;
    frame[6] = payloadLen;
    memcpy(&frame[7], payload, payloadLen);
    frame[frameLen - 2] = 0;
    frame[frameLen - 1] = 0;
    calcCRC(frame, frameLen);

    flush();
    Serial.write(frame, frameLen);

    uint8_t resp[MAX_RESP], respData[MAX_RESP];
    uint8_t respSize = 0, respDataSize = 0;
    int rc = listen(slaveId, resp, respSize, respData, respDataSize);
    if (rc != 0) {
        char lb[48];
        snprintf(lb, sizeof(lb), "WR 0x%04X x%u FAIL rc=%d", reg, regCount, rc);
        appLog.add("MB", lb);
    }
    return rc == 0;
}
