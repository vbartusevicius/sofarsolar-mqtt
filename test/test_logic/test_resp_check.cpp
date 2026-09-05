#include <gtest/gtest.h>
#include "modbus/RespCheck.h"

// The byte count arrives on the wire. Inverter::readSensors passes buffers as
// small as 2 bytes, so an oversized count used to smash the caller's stack.
TEST(RespCheck, AcceptsExactFit) {
    EXPECT_EQ(modbusPayloadLen(46, 51, 46), 46);
    EXPECT_EQ(modbusPayloadLen(2, 7, 2), 2);
}

TEST(RespCheck, AcceptsShortResponseIntoLargerBuffer) {
    EXPECT_EQ(modbusPayloadLen(2, 7, 88), 2);
}

// A late reply to a previous, larger request is the realistic case: it is a
// well-formed frame with a valid CRC, just not the one we asked for.
TEST(RespCheck, RejectsCountLargerThanCallerBuffer) {
    EXPECT_EQ(modbusPayloadLen(88, 93, 2), 0);
    EXPECT_EQ(modbusPayloadLen(48, 53, 46), 0);
}

TEST(RespCheck, RejectsCountLargerThanReceivedFrame) {
    EXPECT_EQ(modbusPayloadLen(46, 20, 46), 0);   // claims 46, only 20 arrived
}

TEST(RespCheck, RejectsZeroCount) {
    EXPECT_EQ(modbusPayloadLen(0, 7, 46), 0);
}

TEST(RespCheck, RejectsMaxCountAgainstSmallBuffer) {
    EXPECT_EQ(modbusPayloadLen(255, 255, 2), 0);
}

TEST(RespCheck, ExpectedFrameAddsOverhead) {
    EXPECT_EQ(modbusExpectedFrame(46, 128), 51);
    EXPECT_EQ(modbusExpectedFrame(2, 128), 7);
}

// 3 + byteCnt + 2 overflowed uint8_t arithmetic for large counts before.
TEST(RespCheck, ExpectedFrameRejectsOverflow) {
    EXPECT_EQ(modbusExpectedFrame(124, 128), 0);
    EXPECT_EQ(modbusExpectedFrame(255, 128), 0);
    EXPECT_EQ(modbusExpectedFrame(0, 128), 0);
}
