#include <gtest/gtest.h>
#include "display/FlowModel.h"

// Worst cases a 22 kWp / 20 kW HYD can produce, plus sensor edge cases.
static InverterData worstCase() {
    InverterData d;
    d.valid = true;
    d.workingMode = WORK_MODE_PASSIVE;
    d.pvPower = 22000; d.pv1Power = 11000; d.pv2Power = 11000;
    d.todayGeneration = 999.9f;
    d.gridPower = -20000;
    d.gridVoltage = 253.4f; d.gridVoltageL2 = 253.4f; d.gridVoltageL3 = 253.4f;
    d.todayImport = 999.9f; d.todayExport = 999.9f;
    d.loadPower = 25000; d.todayConsumption = 999.9f; d.selfSufficiency = 100;
    d.batteryPower = -20000; d.batterySOC = 100; d.battTemp = -20;
    d.inverterTemp = -20; d.heatsinkTemp = -20; d.inverterSelfPower = 12000;
    return d;
}

static UiStatus worstUi() {
    UiStatus s;
    s.saverActive = true;
    s.saverNative = false;
    s.saverTarget = 20000;
    s.saverMaxPower = 20000;
    s.strategyName = "probing";
    s.modeName = "battery_saver";
    s.serial = "SP1ES120N7M558XY";        // 16 chars, the register maximum
    s.firmware = "v2026.10.16.2138";
    s.uptimeS = 999u * 86400u + 23u * 3600u + 59u * 60u;
    s.ip[0] = s.ip[1] = s.ip[2] = s.ip[3] = 255;
    s.rssi = -100;
    s.freeHeap = 81920; s.maxBlock = 81920; s.heapFrag = 100;
    return s;
}

static void expectCardFits(const CardText& c) {
    EXPECT_LE(strlen(c.title) + 1 + strlen(c.hdr), (size_t)FM_CARD_CHARS) << c.title << " " << c.hdr;
    EXPECT_LE(strlen(c.value), (size_t)FM_CARD_BIG) << c.value;
    EXPECT_LE(strlen(c.row1), (size_t)FM_CARD_CHARS) << c.row1;
    EXPECT_LE(strlen(c.row2), (size_t)FM_CARD_CHARS) << c.row2;
}

TEST(FlowModel, CardsFitAtWorstCase) {
    InverterData d = worstCase();
    CardText c;
    flowSolar(d, c); expectCardFits(c);
    flowGrid(d, c);  expectCardFits(c);
    flowHome(d, c);  expectCardFits(c);
    flowBatt(d, c);  expectCardFits(c);
}

// 99.96 kWh prints as "100.0" with %.1f: both counters at that value used to
// produce "In 100.0 Out 100.0" (18 chars) in a 16-char row.
TEST(FlowModel, GridCountersSwitchToIntegersBeforeRoundingUp) {
    InverterData d = worstCase();
    d.todayImport = 99.96f;
    d.todayExport = 99.96f;
    CardText c;
    flowGrid(d, c);
    EXPECT_STREQ(c.row2, "In 100 Out 100");
    d.todayImport = 12.3f; d.todayExport = 4.5f;
    flowGrid(d, c);
    EXPECT_STREQ(c.row2, "In 12.3 Out 4.5");
}

TEST(FlowModel, HubFits) {
    InverterData d = worstCase();
    HubText h;
    flowHub(d, h);
    EXPECT_LE(strlen(h.line1), (size_t)FM_HUB_CHARS);
    EXPECT_LE(strlen(h.line2), (size_t)FM_HUB_CHARS);
}

TEST(FlowModel, ButtonFitsInEveryState) {
    InverterData d = worstCase();
    UiStatus s = worstUi();
    ButtonText b;
    flowButton(d, s, b);
    EXPECT_LE(strlen(b.line2), (size_t)FM_BTN_CHARS) << b.line2;
    s.saverNative = true;
    flowButton(d, s, b);
    EXPECT_LE(strlen(b.line2), (size_t)FM_BTN_CHARS) << b.line2;
    s.saverActive = false;
    flowButton(d, s, b);
    EXPECT_LE(strlen(b.line2), (size_t)FM_BTN_CHARS) << b.line2;
    d.workingMode = 65535;
    flowButton(d, s, b);
    EXPECT_LE(strlen(b.line2), (size_t)FM_BTN_CHARS) << b.line2;
}

// The SN used to share a line with FW and wrapped on the panel.
TEST(FlowModel, SysLinesFitAtWorstCase) {
    InverterData d = worstCase();
    UiStatus s = worstUi();
    char lines[FM_SYS_LINES][64];
    sysLines(d, s, lines);
    for (int i = 0; i < FM_SYS_LINES; i++)
        EXPECT_LE(strlen(lines[i]), (size_t)FM_SYS_CHARS) << lines[i];
}

TEST(FlowModel, DirectionLabelsAndTones) {
    InverterData d;
    CardText c;
    d.gridPower = 500;   flowGrid(d, c); EXPECT_STREQ(c.hdr, "EXPORT"); EXPECT_EQ(c.tone, Tone::Export);
    d.gridPower = -500;  flowGrid(d, c); EXPECT_STREQ(c.hdr, "IMPORT"); EXPECT_EQ(c.tone, Tone::Import);
    EXPECT_STREQ(c.value, "500 W");
    d.batteryPower = -300; flowBatt(d, c); EXPECT_STREQ(c.hdr, "DIS"); EXPECT_EQ(c.tone, Tone::Discharge);
    d.batteryPower = 0;    flowBatt(d, c); EXPECT_STREQ(c.hdr, "IDLE");
}

TEST(FlowModel, NotPassiveWarnsOnlyWithValidData) {
    InverterData d;
    UiStatus s;
    ButtonText b;
    d.workingMode = 0;
    d.valid = false;
    flowButton(d, s, b);
    EXPECT_NE(strncmp(b.line2, "WARN", 4), 0);   // no data yet: no warning
    d.valid = true;
    flowButton(d, s, b);
    EXPECT_EQ(strncmp(b.line2, "WARN", 4), 0);
}

TEST(FlowModel, ArmsFollowSigns) {
    InverterData d;
    FlowArms a;
    d.pvPower = 100; d.gridPower = -50; d.batteryPower = 200; d.loadPower = 0;
    flowArms(d, a);
    EXPECT_TRUE(a.pv);
    EXPECT_TRUE(a.grid);
    EXPECT_TRUE(a.gridToHub);
    EXPECT_FALSE(a.home);
    EXPECT_TRUE(a.batt);
    EXPECT_FALSE(a.battToHub);
}
