#include "Inverter.h"
#include "Config.h"
#include "PassiveWriteCache.h"
#include "util/AppLog.h"

bool Inverter::readBlock(uint16_t start, uint8_t count,
                         uint8_t* buf, uint8_t cap, uint8_t& sz)
{
    return _mb.readHolding(MODBUS_SLAVE_ID, start, count, buf, cap, sz);
}

uint16_t Inverter::u16(const uint8_t* b, uint16_t reg, uint16_t base) {
    uint16_t off = (reg - base) * 2;
    return (uint16_t)((b[off] << 8) | b[off + 1]);
}
int16_t Inverter::s16(const uint8_t* b, uint16_t reg, uint16_t base) {
    uint16_t off = (reg - base) * 2;
    return (int16_t)((b[off] << 8) | b[off + 1]);
}
uint32_t Inverter::u32(const uint8_t* b, uint16_t reg, uint16_t base) {
    uint16_t off = (reg - base) * 2;
    return ((uint32_t)b[off] << 24) | ((uint32_t)b[off+1] << 16) |
           ((uint32_t)b[off+2] << 8) | b[off+3];
}

bool Inverter::readGridBlock() {
    uint8_t grid[88], sz = 0;
    if (!readBlock(REG_GRID_START, REG_GRID_COUNT, grid, sz) || sz < 88)
        return false;
    _data.gridFreq      = u16(grid, REG_GRID_FREQ,    REG_GRID_START) / 100.0f;
    _data.inverterPower = s16(grid, REG_INV_POWER,    REG_GRID_START) * 10L;
    _data.gridPower     = s16(grid, REG_GRID_POWER,   REG_GRID_START) * 10L;
    _data.gridVoltage   = u16(grid, REG_GRID_VOLTAGE, REG_GRID_START) / 10.0f;
    _data.loadPower     = s16(grid, REG_LOAD_POWER,   REG_GRID_START) * 10L;
    _data.sampleSeq++;
    if (_data.sampleSeq == 0) _data.sampleSeq = 1;
    return true;
}

bool Inverter::readPvBlock() {
    uint8_t pv[12], sz = 0;
    if (!readBlock(REG_PV_START, REG_PV_COUNT, pv, sz) || sz < 12)
        return false;
    _data.pv1Voltage = u16(pv, REG_PV1_V, REG_PV_START) / 10.0f;
    _data.pv1Current = u16(pv, REG_PV1_A, REG_PV_START) / 100.0f;
    _data.pv1Power   = u16(pv, REG_PV1_W, REG_PV_START) * 10L;
    _data.pv2Voltage = u16(pv, REG_PV2_V, REG_PV_START) / 10.0f;
    _data.pv2Current = u16(pv, REG_PV2_A, REG_PV_START) / 100.0f;
    _data.pv2Power   = u16(pv, REG_PV2_W, REG_PV_START) * 10L;
    // Sum MPPT1 + MPPT2 directly (10 W resolution) instead of a second round
    // trip to 0x05C4 (100 W resolution, unsupported on some G3 firmwares).
    _data.pvPower    = _data.pv1Power + _data.pv2Power;
    return true;
}

bool Inverter::readBattBlock() {
    uint8_t bat[28], sz = 0;
    if (!readBlock(REG_BATT_START, REG_BATT_COUNT, bat, sz) || sz < 28)
        return false;
    _data.battVoltage  = u16(bat, REG_BATT_V,     REG_BATT_START) / 10.0f;
    _data.battCurrent  = s16(bat, REG_BATT_A,     REG_BATT_START) / 100.0f;
    _data.batteryPower = s16(bat, REG_BATT_W,     REG_BATT_START) * 10L;
    _data.battTemp     = s16(bat, REG_BATT_TEMP,  REG_BATT_START);
    _data.batterySOC   = u16(bat, REG_BATT_SOC,   REG_BATT_START);
    _data.battSOH      = u16(bat, REG_BATT_SOH,   REG_BATT_START);
    _data.battCycles   = u16(bat, REG_BATT_CYC,   REG_BATT_START);
    _data.batt2Voltage = u16(bat, REG_BATT2_V,    REG_BATT_START) / 10.0f;
    _data.batt2Current = s16(bat, REG_BATT2_A,    REG_BATT_START) / 100.0f;
    _data.batt2Power   = s16(bat, REG_BATT2_W,    REG_BATT_START) * 10L;
    _data.batt2Temp    = s16(bat, REG_BATT2_TEMP, REG_BATT_START);
    _data.batt2SOC     = u16(bat, REG_BATT2_SOC,  REG_BATT_START);
    _data.batt2SOH     = u16(bat, REG_BATT2_SOH,  REG_BATT_START);
    _data.batt2Cycles  = u16(bat, REG_BATT2_CYC,  REG_BATT_START);
    return true;
}

bool Inverter::readSysBlock() {
    uint8_t sys[46], sz = 0;
    if (!readBlock(REG_SYS_START, REG_SYS_COUNT, sys, sz) || sz < 46)
        return false;
    _data.runState     = u16(sys, REG_RUNSTATE, REG_SYS_START);
    _data.inverterTemp = s16(sys, REG_INV_TEMP, REG_SYS_START);
    _data.heatsinkTemp = s16(sys, REG_HS_TEMP,  REG_SYS_START);
    return true;
}

bool Inverter::readBattTotBlock() {
    uint8_t bt[6], sz = 0;
    if (!readBlock(REG_BATTTOT_START, REG_BATTTOT_COUNT, bt, sz) || sz < 6)
        return false;
    _data.battTotalPower = s16(bt, REG_BATT_TOT_W,   REG_BATTTOT_START) * 100L;
    _data.battAvgSOC     = u16(bt, REG_BATT_AVG_SOC, REG_BATTTOT_START);
    _data.battAvgSOH     = u16(bt, REG_BATT_AVG_SOH, REG_BATTTOT_START);
    return true;
}

bool Inverter::readEnergyBlock() {
    uint8_t en[48], sz = 0;
    if (!readBlock(REG_ENERGY_START, REG_ENERGY_COUNT, en, sz) || sz < 48)
        return false;
    _data.todayGeneration  = u32(en, REG_GEN_TODAY, REG_ENERGY_START) / 100.0f;
    _data.totalGeneration  = u32(en, REG_GEN_TOTAL, REG_ENERGY_START) / 10.0f;
    _data.todayConsumption = u32(en, REG_USE_TODAY, REG_ENERGY_START) / 100.0f;
    _data.totalConsumption = u32(en, REG_USE_TOTAL, REG_ENERGY_START) / 10.0f;
    _data.todayImport      = u32(en, REG_IMP_TODAY, REG_ENERGY_START) / 100.0f;
    _data.totalImport      = u32(en, REG_IMP_TOTAL, REG_ENERGY_START) / 10.0f;
    _data.todayExport      = u32(en, REG_EXP_TODAY, REG_ENERGY_START) / 100.0f;
    _data.totalExport      = u32(en, REG_EXP_TOTAL, REG_ENERGY_START) / 10.0f;
    _data.todayCharged     = u32(en, REG_CHG_TODAY, REG_ENERGY_START) / 100.0f;
    _data.totalCharged     = u32(en, REG_CHG_TOTAL, REG_ENERGY_START) / 10.0f;
    _data.todayDischarged  = u32(en, REG_DIS_TODAY, REG_ENERGY_START) / 100.0f;
    _data.totalDischarged  = u32(en, REG_DIS_TOTAL, REG_ENERGY_START) / 10.0f;
    return true;
}

bool Inverter::readWorkModeBlock() {
    uint8_t wm[2], sz = 0;
    if (!readBlock(REG_WORKING_MODE, 1, wm, sz) || sz < 2) return false;
    _data.workingMode = u16(wm, REG_WORKING_MODE, REG_WORKING_MODE);
    return true;
}

// Cooperative 1-block-per-tick state machine: keeps each scheduler tick under
// ~60 ms instead of blocking the loop across 4-8 back-to-back Modbus frames.
bool Inverter::pollStep() {
    if (_error) {
        if (!readWorkModeBlock()) {
            _data.valid = false;
            return false;
        }
        _error     = false;
        _pollStep  = POLL_GRID;
        _slowCycle = inverterSlowPollDue(true, _slowTick, SLOW_POLL_DIV);
    }

    if (_pollStep == POLL_GRID) {
        _slowCycle = inverterSlowPollDue(false, _slowTick, SLOW_POLL_DIV);
    }

    bool stepOk = true;
    switch (_pollStep) {
    case POLL_GRID:
        stepOk = readGridBlock();
        _data.valid = stepOk;
        if (!stepOk) { _error = true; return false; }
        break;
    case POLL_PV:
        readPvBlock();
        break;
    case POLL_BATT:
        stepOk = readBattBlock();
        if (!stepOk) { _error = true; return false; }
        break;
    case POLL_SYS:
        readSysBlock();
        break;
    case POLL_BATTTOT:
        readBattTotBlock();
        break;
    case POLL_ENERGY:
        readEnergyBlock();
        break;
    case POLL_WORKMODE:
        readWorkModeBlock();
        break;
    }

    _pollStep = inverterNextPollStep(_pollStep, _slowCycle);
    return stepOk;
}

// Full-cycle synchronous poll (used by tests and one-shot callers)
bool Inverter::readSensors() {
    const bool recovering = _error;
    if (recovering && !readWorkModeBlock()) {
        _data.valid = false;
        return false;
    }

    bool gridOk = readGridBlock();
    yield();
    readPvBlock();
    yield();
    bool battOk = readBattBlock();
    yield();

    if (gridOk && inverterSlowPollDue(recovering, _slowTick, SLOW_POLL_DIV)) {
        readSysBlock();     yield();
        readBattTotBlock(); yield();
        readEnergyBlock();  yield();
        if (!recovering) readWorkModeBlock();
    }

    bool ok = gridOk && battOk;
    _data.valid = gridOk;
    _error = !ok;
    if (ok) {
        char lb[64];
        snprintf(lb, sizeof(lb), "OK soc=%d grid=%d pv=%d batt=%d",
                 _data.batterySOC, (int)_data.gridPower, (int)_data.pvPower, (int)_data.batteryPower);
        appLog.add("INV", lb);
    } else {
        appLog.add("INV", "readSensors fast fail");
    }
    return ok;
}

bool Inverter::readSerialNumber() {
    uint8_t buf[16];
    uint8_t sz;
    if (!readBlock(REG_SN_START, REG_SN_COUNT, buf, sz) || sz < 16) {
        appLog.add("INV", "SN read failed");
        return false;
    }
    for (int i = 0; i < 16; i++) _sn[i] = (char)buf[i];
    _sn[16] = '\0';
    for (int i = 15; i >= 0; i--) {   // trim trailing NULs/spaces
        if (_sn[i] == '\0' || _sn[i] == ' ') _sn[i] = '\0';
        else break;
    }
    return _sn[0] != '\0';
}

// Single choke point for REG_PASSIVE_CTRL writes; suppression policy is in
// passiveWriteDue + saverShouldSend (inverter NVM wear protection)
bool Inverter::sendPassiveCommand(int32_t power) {
    return sendPassiveRange(power, power);
}

void Inverter::setKeepaliveMs(uint32_t ms) {
    if (ms == 0) { _keepaliveMs = 0; return; }         // 0 = never re-send
    _keepaliveMs = ms < 5000UL ? 5000UL : (ms > 600000UL ? 600000UL : ms);
}

// The passive timeout is a register, not an assumption. 0x1184 == 0 means the
// inverter never drops out of passive mode, so an unchanged command never
// needs re-sending; anything else gets re-sent at half the timeout.
bool Inverter::readPassiveTimeout() {
    uint8_t buf[2];
    uint8_t sz = 0;
    if (!readBlock(REG_PASSIVE_TIMEOUT, 1, buf, sz) || sz < 2) return false;
    _passiveTimeoutS = (uint16_t)((buf[0] << 8) | buf[1]);
    if (_passiveTimeoutS == 0xFFFF) _passiveTimeoutS = 0;   // uninitialised

    uint8_t act[2];
    if (readBlock(REG_PASSIVE_TMO_ACT, 1, act, sz) && sz >= 2) {
        _timeoutAction = (uint16_t)((act[0] << 8) | act[1]);
        if (_timeoutAction == 0xFFFF) _timeoutAction = 0;   // uninitialised
    }

    setKeepaliveMs(_passiveTimeoutS ? (uint32_t)_passiveTimeoutS * 500UL : 0);
    char lb[80];
    snprintf(lb, sizeof(lb), "passive timeout=%us action=%u keepalive=%lus",
             (unsigned)_passiveTimeoutS, (unsigned)_timeoutAction,
             (unsigned long)(_keepaliveMs / 1000));
    appLog.add("INV", lb);
    return true;
}

bool Inverter::sendPassiveRange(int32_t minP, int32_t maxP) {
    unsigned long now = millis();
    if (!passiveWriteDue(_cmdCacheValid, _lastSentMin, _lastSentMax,
                         _lastSentAt, minP, maxP, now, _keepaliveMs)) {
        return true;   // unchanged and fresh enough — skip the write
    }

    uint8_t payload[12] = {
        0, 0, 0, 0,
        (uint8_t)((minP >> 24) & 0xFF), (uint8_t)((minP >> 16) & 0xFF),
        (uint8_t)((minP >>  8) & 0xFF), (uint8_t)( minP        & 0xFF),
        (uint8_t)((maxP >> 24) & 0xFF), (uint8_t)((maxP >> 16) & 0xFF),
        (uint8_t)((maxP >>  8) & 0xFF), (uint8_t)( maxP        & 0xFF),
    };

    bool ok = _mb.writeMultiple(MODBUS_SLAVE_ID, REG_PASSIVE_CTRL, 6, payload, 12);
    if (ok) {
        _lastSentMin   = minP;
        _lastSentMax   = maxP;
        _lastSentAt    = now;
        _cmdCacheValid = true;
    } else {
        _cmdCacheValid = false;   // force retry on next reconcile/update tick
    }
    return ok;
}

// Reads back 0x1187..0x118C (Gdes, Blo, Bup) to confirm the inverter accepted
// and retained the asymmetric [expectedMin, expectedMax] passive window.
bool Inverter::verifyPassiveRange(int32_t expectedMin, int32_t expectedMax) {
    uint8_t buf[12], sz = 0;
    if (!readBlock(REG_PASSIVE_CTRL, 6, buf, sz) || sz < 12) return false;
    int32_t gdes = (int32_t)u32(buf, REG_PASSIVE_CTRL,     REG_PASSIVE_CTRL);
    int32_t blo  = (int32_t)u32(buf, REG_PASSIVE_CTRL + 2, REG_PASSIVE_CTRL);
    int32_t bup  = (int32_t)u32(buf, REG_PASSIVE_CTRL + 4, REG_PASSIVE_CTRL);
    return gdes == 0 && blo == expectedMin && bup == expectedMax;
}
