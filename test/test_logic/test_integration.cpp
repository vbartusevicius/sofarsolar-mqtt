#include <gtest/gtest.h>
#include "inverter/TelemetrySchema.h"

// Compile the real production implementation files against our hardware fakes
// to test the full Modbus -> Inverter -> BatterySaver / ModeController stack.
#include "util/AppLog.cpp"
#include "modbus/Modbus.cpp"
#include "inverter/Inverter.cpp"
#include "control/BatterySaver.cpp"
#include "control/ModeController.cpp"

namespace {

struct SchemaEntry {
    const char* key;
    const char* name;
    const char* unit;
    const char* devClass;
    const char* stateClass;
    int         decimals;
};

#define SCHEMA_INT_ENTRY(k, m, n, u, dc, sc)      { k, n, u, dc, sc, 0 },
#define SCHEMA_FLT_ENTRY(k, m, dec, n, u, dc, sc) { k, n, u, dc, sc, dec },

static const SchemaEntry ALL_SCHEMA[] = {
    SOFAR_TELEMETRY_INT_FIELDS(SCHEMA_INT_ENTRY)
    SOFAR_TELEMETRY_FLOAT_FIELDS(SCHEMA_FLT_ENTRY)
};

// Simulated Sofar HYD inverter responder on the fake RS485 bus
struct FakeInverterBus {
    static int16_t  gridPowerW;
    static int16_t  battPowerW;
    static uint16_t batterySoc;
    static uint16_t workingMode;
    static bool     corruptNextWrite;
    static bool     allowNativeWindow;
    static uint32_t fc16Count;
    static int32_t  lastMinWritten;
    static int32_t  lastMaxWritten;

    static void reset() {
        Serial.reset();
        fakeClock().now   = 10000;
        gridPowerW        = 0;
        battPowerW        = 0;
        batterySoc        = 75;
        workingMode       = WORK_MODE_PASSIVE;
        corruptNextWrite  = false;
        allowNativeWindow = true;
        fc16Count         = 0;
        lastMinWritten    = -999999;
        lastMaxWritten    = -999999;
        Serial.onWrite    = &FakeInverterBus::onMasterTx;
    }

    static void appendCrcAndEnqueue(std::vector<uint8_t>& frame) {
        uint16_t crc = modbusCrc16(frame.data(), frame.size());
        frame.push_back((uint8_t)(crc & 0xFF));
        frame.push_back((uint8_t)(crc >> 8));
        Serial.rx.insert(Serial.rx.end(), frame.begin(), frame.end());
    }

    static int32_t readS32BE(const uint8_t* p) {
        uint32_t u = ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
                     ((uint32_t)p[2] << 8)  | (uint32_t)p[3];
        return (int32_t)u;
    }

    static void writeS32BE(uint8_t* p, int32_t v) {
        uint32_t u = (uint32_t)v;
        p[0] = (uint8_t)((u >> 24) & 0xFF);
        p[1] = (uint8_t)((u >> 16) & 0xFF);
        p[2] = (uint8_t)((u >>  8) & 0xFF);
        p[3] = (uint8_t)( u        & 0xFF);
    }

    static void onMasterTx(const uint8_t* req, size_t len) {
        if (len < 8) return;
        uint8_t slave = req[0];
        uint8_t fc    = req[1];
        uint16_t reg  = (uint16_t)((req[2] << 8) | req[3]);

        if (fc == FC_READ_HOLDING && len == 8) {
            uint8_t count = req[5];
            uint8_t bytes = (uint8_t)(count * 2);
            std::vector<uint8_t> resp = { slave, FC_READ_HOLDING, bytes };
            resp.resize(3 + bytes, 0);

            if (reg == REG_GRID_START && count == REG_GRID_COUNT) {
                int16_t rawGrid = (int16_t)(gridPowerW / 10);
                size_t off = 3 + (REG_GRID_POWER - REG_GRID_START) * 2;
                resp[off]     = (uint8_t)((rawGrid >> 8) & 0xFF);
                resp[off + 1] = (uint8_t)(rawGrid & 0xFF);
            } else if (reg == REG_BATT_START && count == REG_BATT_COUNT) {
                int16_t rawBatt = (int16_t)(battPowerW / 10);
                size_t wOff = 3 + (REG_BATT_W - REG_BATT_START) * 2;
                resp[wOff]     = (uint8_t)((rawBatt >> 8) & 0xFF);
                resp[wOff + 1] = (uint8_t)(rawBatt & 0xFF);
                size_t off = 3 + (REG_BATT_SOC - REG_BATT_START) * 2;
                resp[off]     = (uint8_t)((batterySoc >> 8) & 0xFF);
                resp[off + 1] = (uint8_t)(batterySoc & 0xFF);
            } else if (reg == REG_WORKING_MODE && count == 1) {
                resp[3] = (uint8_t)((workingMode >> 8) & 0xFF);
                resp[4] = (uint8_t)(workingMode & 0xFF);
            } else if (reg == REG_PASSIVE_CTRL && count == 6) {
                int32_t retMin = allowNativeWindow ? lastMinWritten : lastMaxWritten;
                writeS32BE(&resp[3],  0);
                writeS32BE(&resp[7],  retMin);
                writeS32BE(&resp[11], lastMaxWritten);
            }
            appendCrcAndEnqueue(resp);
        } else if (fc == FC_WRITE_MULTI && reg == REG_PASSIVE_CTRL && len == 21) {
            fc16Count++;
            int32_t reqMin = readS32BE(&req[11]);
            int32_t reqMax = readS32BE(&req[15]);
            if (corruptNextWrite || (!allowNativeWindow && reqMin != reqMax)) {
                corruptNextWrite = false;
                // Enqueue a Modbus exception / corrupt frame so writeMultiple fails
                Serial.rx.push_back(slave);
                Serial.rx.push_back((uint8_t)(FC_WRITE_MULTI | 0x80));
                return;
            }
            lastMinWritten = reqMin;
            lastMaxWritten = reqMax;
            std::vector<uint8_t> resp = {
                slave, FC_WRITE_MULTI, req[2], req[3], req[4], req[5]
            };
            appendCrcAndEnqueue(resp);
        }
    }
};

int16_t  FakeInverterBus::gridPowerW        = 0;
int16_t  FakeInverterBus::battPowerW        = 0;
uint16_t FakeInverterBus::batterySoc        = 75;
uint16_t FakeInverterBus::workingMode       = WORK_MODE_PASSIVE;
bool     FakeInverterBus::corruptNextWrite  = false;
bool     FakeInverterBus::allowNativeWindow = true;
uint32_t FakeInverterBus::fc16Count         = 0;
int32_t  FakeInverterBus::lastMinWritten    = -999999;
int32_t  FakeInverterBus::lastMaxWritten    = -999999;

} // namespace

TEST(TelemetrySchema, AllKeysAndNamesAreUniqueAndComplete) {
    constexpr size_t N = sizeof(ALL_SCHEMA) / sizeof(ALL_SCHEMA[0]);
    EXPECT_EQ(N, 48u);
    for (size_t i = 0; i < N; i++) {
        ASSERT_NE(ALL_SCHEMA[i].key, nullptr);
        ASSERT_NE(ALL_SCHEMA[i].name, nullptr);
        EXPECT_GT(strlen(ALL_SCHEMA[i].key), 0u);
        EXPECT_GT(strlen(ALL_SCHEMA[i].name), 0u);
        for (size_t j = i + 1; j < N; j++) {
            EXPECT_STRNE(ALL_SCHEMA[i].key, ALL_SCHEMA[j].key);
            EXPECT_STRNE(ALL_SCHEMA[i].name, ALL_SCHEMA[j].name);
        }
    }
}

TEST(Integration, OptionANativeWindowUsedWhenReadbackSucceeds) {
    FakeInverterBus::reset();
    FakeInverterBus::allowNativeWindow = true;
    FakeInverterBus::gridPowerW = 0;
    FakeInverterBus::battPowerW = 1200;

    Modbus mb;
    Inverter inv(mb);
    BatterySaver bs(inv);
    EEConfig cfg;
    cfg.begin();
    ModeController ctrl(inv, bs, cfg);

    ctrl.setMode("battery_saver");
    ASSERT_TRUE(inv.pollStep());   // POLL_GRID

    // First update probes Option A [0, 20000] and verifies readback -> BSAVE_NATIVE
    bs.update();
    EXPECT_EQ(bs.strategy(), BatterySaver::BSAVE_NATIVE);
    EXPECT_STREQ(bs.strategyName(), "native");
    EXPECT_EQ(FakeInverterBus::fc16Count, 1u);
    EXPECT_EQ(FakeInverterBus::lastMinWritten, 0);
    EXPECT_EQ(FakeInverterBus::lastMaxWritten, 20000);

    // Subsequent polls while inverter tracks properly cause ZERO additional writes
    for (int i = 0; i < 12; i++) {
        fakeClock().now += 1000;
        inv.pollStep();
        bs.update();
    }
    EXPECT_EQ(bs.strategy(), BatterySaver::BSAVE_NATIVE);
    EXPECT_EQ(FakeInverterBus::fc16Count, 1u);
}

TEST(Integration, OptionBFallbackAndSingleIntegrationWhenReadbackFails) {
    FakeInverterBus::reset();
    FakeInverterBus::allowNativeWindow = false;   // inverter rejects Blo != Bup
    FakeInverterBus::gridPowerW = 600;            // 600 W export

    Modbus mb;
    Inverter inv(mb);
    BatterySaver bs(inv);
    EEConfig cfg;
    cfg.begin();
    ModeController ctrl(inv, bs, cfg);

    ctrl.setMode("battery_saver");
    EXPECT_EQ(FakeInverterBus::fc16Count, 0u);

    // Step 1: link recovery probe + POLL_GRID -> reads 600 W export, bumps sampleSeq
    ASSERT_TRUE(inv.pollStep());
    EXPECT_TRUE(inv.isPassiveMode());
    EXPECT_EQ(inv.data().gridPower, 600);
    EXPECT_EQ(inv.data().sampleSeq, 1u);

    // Probing [0, 20000] is retried 3 times; on the 3rd rejection it falls back
    // to Option B loop in the same update() and sends (600, 600)
    bs.update();
    EXPECT_EQ(bs.strategy(), BatterySaver::BSAVE_PROBING);
    bs.update();
    EXPECT_EQ(bs.strategy(), BatterySaver::BSAVE_PROBING);
    bs.update();
    EXPECT_EQ(bs.strategy(), BatterySaver::BSAVE_LOOP);
    EXPECT_STREQ(bs.strategyName(), "loop");
    EXPECT_EQ(bs.targetPower(), 600);
    EXPECT_EQ(FakeInverterBus::fc16Count, 4u);
    EXPECT_EQ(FakeInverterBus::lastMinWritten, 600);
    EXPECT_EQ(FakeInverterBus::lastMaxWritten, 600);

    // Calling bs.update() again before the next Grid block must NOT double-integrate
    fakeClock().now += 3000;
    bs.update();
    EXPECT_EQ(bs.targetPower(), 600);
    EXPECT_EQ(FakeInverterBus::fc16Count, 4u);

    // Switching from battery_saver to charge(2500) must emit ONE write, not 0 W then 2500 W
    ctrl.setCharge(2500);
    EXPECT_EQ(FakeInverterBus::fc16Count, 5u);
    EXPECT_EQ(FakeInverterBus::lastMinWritten, 2500);
    EXPECT_EQ(FakeInverterBus::lastMaxWritten, 2500);
}

TEST(Integration, OptionAFallsBackToLoopWhenInverterIgnoresWindowAtRuntime) {
    FakeInverterBus::reset();
    FakeInverterBus::allowNativeWindow = true;
    FakeInverterBus::gridPowerW = 600;   // exporting 600 W
    FakeInverterBus::battPowerW = 0;     // ...while battery sits at 0 W (not tracking!)
    FakeInverterBus::batterySoc = 60;

    Modbus mb;
    Inverter inv(mb);
    BatterySaver bs(inv);
    EEConfig cfg;
    cfg.begin();
    ModeController ctrl(inv, bs, cfg);

    ctrl.setMode("battery_saver");
    ASSERT_TRUE(inv.pollStep());
    bs.update();
    ASSERT_EQ(bs.strategy(), BatterySaver::BSAVE_NATIVE);
    EXPECT_EQ(FakeInverterBus::fc16Count, 1u);

    // Run poll cycles until BSAVE_NATIVE_FAIL_STREAK fresh Grid samples arrive
    for (int i = 0; i < 60 && bs.strategy() == BatterySaver::BSAVE_NATIVE; i++) {
        fakeClock().now += 1000;
        inv.pollStep();
        bs.update();
    }
    EXPECT_EQ(bs.strategy(), BatterySaver::BSAVE_LOOP);
    EXPECT_EQ(bs.targetPower(), 600);
    EXPECT_EQ(FakeInverterBus::lastMinWritten, 600);
    EXPECT_EQ(FakeInverterBus::lastMaxWritten, 600);
}

TEST(Integration, ReconcileRetriesFailedWriteAndStopsOnceSynced) {
    FakeInverterBus::reset();
    Modbus mb;
    Inverter inv(mb);
    BatterySaver bs(inv);
    EEConfig cfg;
    cfg.begin();
    ModeController ctrl(inv, bs, cfg);

    // Bring inverter online
    ASSERT_TRUE(inv.pollStep());

    // First attempt to set auto(8000) hits an RS485 CRC/timeout glitch
    FakeInverterBus::corruptNextWrite = true;
    ctrl.setAuto(8000);
    EXPECT_EQ(FakeInverterBus::fc16Count, 1u);
    EXPECT_NE(FakeInverterBus::lastMaxWritten, 8000);

    // Next reconcile() tick retries automatically and succeeds
    fakeClock().now += 3000;
    ctrl.reconcile();
    EXPECT_EQ(FakeInverterBus::fc16Count, 2u);
    EXPECT_EQ(FakeInverterBus::lastMinWritten, -8000);
    EXPECT_EQ(FakeInverterBus::lastMaxWritten,  8000);

    // Subsequent reconcile() ticks do not write to the inverter at all
    for (int i = 0; i < 10; i++) {
        fakeClock().now += 3000;
        ctrl.reconcile();
    }
    EXPECT_EQ(FakeInverterBus::fc16Count, 2u);
}
