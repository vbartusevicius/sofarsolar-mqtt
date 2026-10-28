#include <gtest/gtest.h>
#include "control/Mode.h"
#include "control/Command.h"

TEST(Mode, NamesRoundTrip) {
    for (Mode m : { Mode::Auto, Mode::Charge, Mode::Standby, Mode::BatterySaver }) {
        Mode back = Mode::Auto;
        ASSERT_TRUE(modeParse(modeName(m), back));
        EXPECT_EQ(back, m);
    }
}

TEST(Mode, ParseIsExactNotPrefix) {
    Mode m = Mode::Auto;
    EXPECT_FALSE(modeParse("chargeX", m));
    EXPECT_FALSE(modeParse("stop", m));
    EXPECT_FALSE(modeParse("au", m));
    EXPECT_FALSE(modeParse("", m));
    EXPECT_FALSE(modeParse(nullptr, m));
}

TEST(Mode, MapsStaticModesAndSkipsSaver) {
    int32_t lo = -1, hi = -1;
    EXPECT_TRUE(modeDesiredPassiveRange(Mode::Charge, 3500, 16384, lo, hi));
    EXPECT_EQ(lo, 3500);
    EXPECT_EQ(hi, 3500);
    EXPECT_TRUE(modeDesiredPassiveRange(Mode::Standby, 3500, 16384, lo, hi));
    EXPECT_EQ(lo, 0);
    EXPECT_EQ(hi, 0);
    EXPECT_TRUE(modeDesiredPassiveRange(Mode::Auto, 3500, 8000, lo, hi));
    EXPECT_EQ(lo, -8000);
    EXPECT_EQ(hi, 8000);
    EXPECT_FALSE(modeDesiredPassiveRange(Mode::BatterySaver, 3500, 8000, lo, hi));
}

// LCD, web and Home Assistant all used to disagree on what "off" means.
TEST(Mode, SaverOffAlwaysReturnsToAuto) {
    EXPECT_EQ(modeForSaverSwitch(Mode::BatterySaver, false), Mode::Auto);
    EXPECT_EQ(modeForSaverSwitch(Mode::Auto, true), Mode::BatterySaver);
    EXPECT_EQ(modeForSaverSwitch(Mode::Standby, true), Mode::BatterySaver);
}

TEST(Mode, SaverOffLeavesOtherModesAlone) {
    EXPECT_EQ(modeForSaverSwitch(Mode::Charge, false), Mode::Charge);
    EXPECT_EQ(modeForSaverSwitch(Mode::Standby, false), Mode::Standby);
}

TEST(Command, BatterySaveSwitchAndToggle) {
    Command c;
    ASSERT_TRUE(parseCommand("battery_save", "on", c));
    EXPECT_EQ(c.kind, CmdKind::SaverSwitch);
    EXPECT_TRUE(c.on);
    ASSERT_TRUE(parseCommand("battery_save", "OFF", c));
    EXPECT_FALSE(c.on);
    ASSERT_TRUE(parseCommand("battery_save", "", c));
    EXPECT_EQ(c.kind, CmdKind::SaverToggle);
    EXPECT_FALSE(parseCommand("battery_save", "maybe", c));
}

TEST(Command, ModeAndStandby) {
    Command c;
    ASSERT_TRUE(parseCommand("mode", "charge", c));
    EXPECT_EQ(c.kind, CmdKind::SetMode);
    EXPECT_EQ(c.mode, Mode::Charge);
    EXPECT_FALSE(parseCommand("mode", "bogus", c));
    ASSERT_TRUE(parseCommand("standby", "anything", c));
    EXPECT_EQ(c.mode, Mode::Standby);
}

TEST(Command, NumbersAreValidatedAndClamped) {
    Command c;
    ASSERT_TRUE(parseCommand("charge", "2500", c));
    EXPECT_EQ(c.kind, CmdKind::SetCharge);
    EXPECT_EQ(c.value, 2500);
    ASSERT_TRUE(parseCommand("charge", "-99999", c));
    EXPECT_EQ(c.value, -BSAVE_MAX_POWER);
    EXPECT_FALSE(parseCommand("charge", "12abc", c));
    EXPECT_FALSE(parseCommand("charge", "", c));

    ASSERT_TRUE(parseCommand("auto", "8000", c));
    EXPECT_EQ(c.value, 8000);
    ASSERT_TRUE(parseCommand("auto", "0", c));
    EXPECT_EQ(c.value, 100);
    ASSERT_TRUE(parseCommand("auto", "", c));
    EXPECT_EQ(c.value, AUTO_LIMIT_DEFAULT);
}

TEST(Command, UnknownKeyRejected) {
    Command c;
    EXPECT_FALSE(parseCommand("reboot", "1", c));
    EXPECT_FALSE(parseCommand(nullptr, "1", c));
}

TEST(Command, MqttTopicKey) {
    EXPECT_STREQ(mqttCommandKey("sofar_X/set/mode"), "mode");
    EXPECT_STREQ(mqttCommandKey("a/b/set/battery_save"), "battery_save");
    EXPECT_EQ(mqttCommandKey("sofar_X/ping"), nullptr);
    EXPECT_EQ(mqttCommandKey("sofar_X/set/"), nullptr);
    EXPECT_EQ(mqttCommandKey(nullptr), nullptr);
}
