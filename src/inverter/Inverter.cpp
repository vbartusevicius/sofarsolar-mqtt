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

// Stage-and-swap: commit only the full read, never a mix of old/new values
bool Inverter::readSensors() {
    bool ok = true;
    uint8_t sz;

    if (_error) {
        uint8_t probe[2];
        if (!readBlock(REG_WORKING_MODE, 1, probe, sz) || sz < 2) {
            _data.valid = false;      // never let stale values look current
            return false;
        }
    }

    InverterData next = _data;

    // System block (23 regs)
    uint8_t sys[46];
    if (readBlock(REG_SYS_START, REG_SYS_COUNT, sys, sz) && sz >= 46) {
        next.runState     = u16(sys, REG_RUNSTATE, REG_SYS_START);
        next.inverterTemp = s16(sys, REG_INV_TEMP, REG_SYS_START);
        next.heatsinkTemp = s16(sys, REG_HS_TEMP,  REG_SYS_START);
    } else { ok = false; }
    yield();

    // Grid block (44 regs)
    uint8_t grid[88];
    if (readBlock(REG_GRID_START, REG_GRID_COUNT, grid, sz) && sz >= 88) {
        next.gridFreq      = u16(grid, REG_GRID_FREQ,    REG_GRID_START) / 100.0f;
        next.inverterPower = s16(grid, REG_INV_POWER,    REG_GRID_START) * 10L;
        next.gridPower     = s16(grid, REG_GRID_POWER,   REG_GRID_START) * 10L;
        next.gridVoltage   = u16(grid, REG_GRID_VOLTAGE, REG_GRID_START) / 10.0f;
        next.loadPower     = s16(grid, REG_LOAD_POWER,   REG_GRID_START) * 10L;
    } else { ok = false; }
    yield();

    // PV block (6 regs)
    uint8_t pv[12];
    if (readBlock(REG_PV_START, REG_PV_COUNT, pv, sz) && sz >= 12) {
        next.pv1Voltage = u16(pv, REG_PV1_V, REG_PV_START) / 10.0f;
        next.pv1Current = u16(pv, REG_PV1_A, REG_PV_START) / 100.0f;
        next.pv1Power   = u16(pv, REG_PV1_W, REG_PV_START) * 10L;
        next.pv2Voltage = u16(pv, REG_PV2_V, REG_PV_START) / 10.0f;
        next.pv2Current = u16(pv, REG_PV2_A, REG_PV_START) / 100.0f;
        next.pv2Power   = u16(pv, REG_PV2_W, REG_PV_START) * 10L;
    } else { ok = false; }
    yield();

    // PV total (1 reg, separate block)
    uint8_t pvt[2];
    if (readBlock(REG_PV_TOTAL, 1, pvt, sz) && sz >= 2) {
        next.pvPower = u16(pvt, REG_PV_TOTAL, REG_PV_TOTAL) * 100L;
    } else { ok = false; }
    yield();

    // Battery block (14 regs)
    uint8_t bat[28];
    if (readBlock(REG_BATT_START, REG_BATT_COUNT, bat, sz) && sz >= 28) {
        next.battVoltage  = u16(bat, REG_BATT_V,    REG_BATT_START) / 10.0f;
        next.battCurrent  = s16(bat, REG_BATT_A,    REG_BATT_START) / 100.0f;
        next.batteryPower = s16(bat, REG_BATT_W,    REG_BATT_START) * 10L;
        next.battTemp     = s16(bat, REG_BATT_TEMP, REG_BATT_START);
        next.batterySOC   = u16(bat, REG_BATT_SOC,  REG_BATT_START);
        next.battSOH      = u16(bat, REG_BATT_SOH,  REG_BATT_START);
        next.battCycles   = u16(bat, REG_BATT_CYC,  REG_BATT_START);
        next.batt2Voltage = u16(bat, REG_BATT2_V,   REG_BATT_START) / 10.0f;
        next.batt2Current = s16(bat, REG_BATT2_A,   REG_BATT_START) / 100.0f;
        next.batt2Power   = s16(bat, REG_BATT2_W,   REG_BATT_START) * 10L;
        next.batt2Temp    = s16(bat, REG_BATT2_TEMP,REG_BATT_START);
        next.batt2SOC     = u16(bat, REG_BATT2_SOC, REG_BATT_START);
        next.batt2SOH     = u16(bat, REG_BATT2_SOH, REG_BATT_START);
        next.batt2Cycles  = u16(bat, REG_BATT2_CYC, REG_BATT_START);
    } else { ok = false; }
    yield();

    // Battery totals (3 regs)
    uint8_t bt[6];
    if (readBlock(REG_BATTTOT_START, REG_BATTTOT_COUNT, bt, sz) && sz >= 6) {
        next.battTotalPower = s16(bt, REG_BATT_TOT_W,   REG_BATTTOT_START) * 100L;
        next.battAvgSOC     = u16(bt, REG_BATT_AVG_SOC, REG_BATTTOT_START);
        next.battAvgSOH     = u16(bt, REG_BATT_AVG_SOH, REG_BATTTOT_START);
    } else { ok = false; }
    yield();

    // Energy block (24 regs, U32 values)
    uint8_t en[48];
    if (readBlock(REG_ENERGY_START, REG_ENERGY_COUNT, en, sz) && sz >= 48) {
        next.todayGeneration  = u32(en, REG_GEN_TODAY, REG_ENERGY_START) / 100.0f;
        next.totalGeneration  = u32(en, REG_GEN_TOTAL, REG_ENERGY_START) / 10.0f;
        next.todayConsumption = u32(en, REG_USE_TODAY, REG_ENERGY_START) / 100.0f;
        next.totalConsumption = u32(en, REG_USE_TOTAL, REG_ENERGY_START) / 10.0f;
        next.todayImport      = u32(en, REG_IMP_TODAY, REG_ENERGY_START) / 100.0f;
        next.totalImport      = u32(en, REG_IMP_TOTAL, REG_ENERGY_START) / 10.0f;
        next.todayExport      = u32(en, REG_EXP_TODAY, REG_ENERGY_START) / 100.0f;
        next.totalExport      = u32(en, REG_EXP_TOTAL, REG_ENERGY_START) / 10.0f;
        next.todayCharged     = u32(en, REG_CHG_TODAY, REG_ENERGY_START) / 100.0f;
        next.totalCharged     = u32(en, REG_CHG_TOTAL, REG_ENERGY_START) / 10.0f;
        next.todayDischarged  = u32(en, REG_DIS_TODAY, REG_ENERGY_START) / 100.0f;
        next.totalDischarged  = u32(en, REG_DIS_TOTAL, REG_ENERGY_START) / 10.0f;
    } else { ok = false; }
    yield();

    // Working mode (1 reg)
    uint8_t wm[2];
    if (readBlock(REG_WORKING_MODE, 1, wm, sz) && sz >= 2) {
        next.workingMode = u16(wm, REG_WORKING_MODE, REG_WORKING_MODE);
    } else { ok = false; }

    if (ok) _data = next;
    _data.valid = ok;
    _error = !ok;
    if (ok) {
        char lb[64];
        snprintf(lb, sizeof(lb), "OK soc=%d grid=%d pv=%d batt=%d",
                 _data.batterySOC, (int)_data.gridPower, (int)_data.pvPower, (int)_data.batteryPower);
        appLog.add("INV", lb);
    } else {
        appLog.add("INV", "readSensors partial fail");
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
    if (readBlock(REG_PASSIVE_TMO_ACT, 1, act, sz) && sz >= 2)
        _timeoutAction = (uint16_t)((act[0] << 8) | act[1]);

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
    }
    return ok;
}
