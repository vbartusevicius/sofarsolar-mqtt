#include <gtest/gtest.h>
#include "modbus/Crc16.h"

// Reference frames from the Modbus application protocol specification and
// cross-checked with an independent Python implementation.

TEST(Crc16, ReadHoldingRequest) {
    const uint8_t frame[] = {0x11, 0x03, 0x00, 0x6B, 0x00, 0x03};
    EXPECT_EQ(modbusCrc16(frame, sizeof(frame)), 0x8776);
}

TEST(Crc16, ReadHoldingResponse) {
    const uint8_t frame[] = {0x01, 0x03, 0x04, 0x04, 0x00, 0x17};
    EXPECT_EQ(modbusCrc16(frame, sizeof(frame)), 0x3545);
}

TEST(Crc16, PassiveCtrlWriteRequest) {
    // FC0x10 write of [0, 2000, 2000] to 0x1187 — the battery-save command
    const uint8_t frame[] = {0x01, 0x10, 0x11, 0x87, 0x00, 0x06, 0x0C,
                             0, 0, 0, 0, 0, 0, 0x07, 0xD0, 0, 0, 0x07, 0xD0};
    EXPECT_EQ(modbusCrc16(frame, sizeof(frame)), 0xD9B6);
}

TEST(Crc16, EmptyInputIsInitialValue) {
    EXPECT_EQ(modbusCrc16(nullptr, 0), 0xFFFF);
}
