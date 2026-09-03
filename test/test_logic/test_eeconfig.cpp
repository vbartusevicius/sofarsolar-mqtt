#include <gtest/gtest.h>

// Compile EEConfig against the fakes (test/fakes/{EEPROM,WiFi,Arduino}.h)
#include "config/EEConfig.cpp"

class EEConfigTest : public ::testing::Test {
protected:
    EEConfig cfg;
    void SetUp() override {
        cfg.begin();   // fills fake EEPROM with 0xFF, resets counters
    }
};

TEST_F(EEConfigTest, ErasedFlashLoadsDefaults) {
    EXPECT_FALSE(cfg.load());
    EXPECT_STREQ(cfg.name(), "Sofar");
    EXPECT_STREQ(cfg.mqttPort(), "1883");
    EXPECT_EQ(cfg.bsaveDelta(),    BSAVE_MIN_DELTA);
    EXPECT_EQ(cfg.bsaveMaxPower(), BSAVE_MAX_POWER);
    EXPECT_EQ(cfg.keepaliveMs(),   (uint32_t)PASSIVE_KEEPALIVE_MS);
    EXPECT_EQ(cfg.idleLapseMs(),   (uint32_t)BSAVE_IDLE_LAPSE_MS);
}

TEST_F(EEConfigTest, UnchangedSaveDoesNotCommitToFlash) {
    // First save writes everything (fresh flash)
    cfg.save();
    EXPECT_EQ(EEPROM.commitCalls, 1u);
    const uint32_t writesAfterFirstSave = EEPROM.writeCalls;
    EXPECT_GT(writesAfterFirstSave, 0u);

    // Saving identical values must not touch flash at all — this is the
    // dirty-guard that protects the ESP8266's flash sector.
    cfg.save();
    cfg.save();
    EXPECT_EQ(EEPROM.commitCalls, 1u);
    EXPECT_EQ(EEPROM.writeCalls, writesAfterFirstSave);
}

TEST_F(EEConfigTest, ChangedFieldCommitsOnce) {
    cfg.save();
    cfg.mqttHost()[0] = 'b'; cfg.mqttHost()[1] = '\0';   // "" → "b"
    cfg.save();
    EXPECT_EQ(EEPROM.commitCalls, 2u);
}

TEST_F(EEConfigTest, SaveLoadRoundtrip) {
    strncpy(cfg.name(), "tester", EE_NAME_LEN);
    strncpy(cfg.mqttHost(), "192.168.1.10", EE_HOST_LEN);
    cfg.setBsaveDelta(250);
    cfg.setKeepaliveMs(30000);
    cfg.save();

    // Fake flash persists across instances (no begin() = no re-erase)
    EEConfig fresh;
    EXPECT_TRUE(fresh.load());
    EXPECT_STREQ(fresh.name(), "tester");
    EXPECT_STREQ(fresh.mqttHost(), "192.168.1.10");
    EXPECT_EQ(fresh.bsaveDelta(), 250);
    EXPECT_EQ(fresh.keepaliveMs(), 30000u);
    EXPECT_STREQ(WiFi.hostnameBuf, "tester");
}

TEST_F(EEConfigTest, TuningDefaultsWithLegacyMagicOnly) {
    // Simulate an EEPROM written by older firmware: magic + strings set,
    // tuning area still erased (0xFF → readEE32() == -1 → keep defaults)
    cfg.save();   // writes everything incl. tuning
    // erase just the tuning fields back to 0xFF
    for (int off = EE_BSAVE_DELTA; off < EE_IDLE_LAPSE_MS + 4; off++)
        EEPROM.mem[off] = 0xFF;

    EEConfig fresh;
    EXPECT_TRUE(fresh.load());
    EXPECT_EQ(fresh.bsaveDelta(), BSAVE_MIN_DELTA);
    EXPECT_EQ(fresh.keepaliveMs(), (uint32_t)PASSIVE_KEEPALIVE_MS);
}

TEST_F(EEConfigTest, ReadHandlesMissingNullTerminator) {
    // 64 bytes with no NUL: readEE must truncate at len-1 and terminate
    EEPROM.mem[EE_MAGIC] = '1';
    for (int i = 0; i < EE_NAME_LEN; i++) EEPROM.mem[EE_NAME + i] = 'x';
    EEConfig fresh;
    EXPECT_TRUE(fresh.load());
    EXPECT_EQ(strlen(fresh.name()), (size_t)(EE_NAME_LEN - 1));
}
