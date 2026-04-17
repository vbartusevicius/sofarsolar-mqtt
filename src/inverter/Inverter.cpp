#include "Inverter.h"
#include "Config.h"

// ── Bulk-read wrapper ─────────────────────────────────────────
bool Inverter::readBlock(uint16_t start, uint8_t count,
                         uint8_t* buf, uint8_t& sz)
{
    return _mb.readHolding(MODBUS_SLAVE_ID, start, count, buf, sz);
}

// ── Value extraction from bulk response ────────────────────────
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

// ── Read all sensor register blocks ───────────────────────────
bool Inverter::readSensors() {
    bool ok = true;
    uint8_t sz;

    // System block (23 regs)
    uint8_t sys[46];
    if (readBlock(REG_SYS_START, REG_SYS_COUNT, sys, sz) && sz >= 46) {
        _data.runState     = u16(sys, REG_RUNSTATE, REG_SYS_START);
        _data.inverterTemp = s16(sys, REG_INV_TEMP, REG_SYS_START);
        _data.heatsinkTemp = s16(sys, REG_HS_TEMP,  REG_SYS_START);
    } else { ok = false; }

    // Grid block (44 regs)
    uint8_t grid[88];
    if (readBlock(REG_GRID_START, REG_GRID_COUNT, grid, sz) && sz >= 88) {
        _data.gridFreq      = u16(grid, REG_GRID_FREQ,    REG_GRID_START) / 100.0f;
        _data.inverterPower = s16(grid, REG_INV_POWER,    REG_GRID_START) * 10L;
        _data.gridPower     = s16(grid, REG_GRID_POWER,   REG_GRID_START) * 10L;
        _data.gridVoltage   = u16(grid, REG_GRID_VOLTAGE, REG_GRID_START) / 10.0f;
        _data.loadPower     = s16(grid, REG_LOAD_POWER,   REG_GRID_START) * 10L;
    } else { ok = false; }

    // PV block (6 regs)
    uint8_t pv[12];
    if (readBlock(REG_PV_START, REG_PV_COUNT, pv, sz) && sz >= 12) {
        _data.pv1Voltage = u16(pv, REG_PV1_V, REG_PV_START) / 10.0f;
        _data.pv1Current = u16(pv, REG_PV1_A, REG_PV_START) / 100.0f;
        _data.pv1Power   = u16(pv, REG_PV1_W, REG_PV_START) * 10L;
        _data.pv2Voltage = u16(pv, REG_PV2_V, REG_PV_START) / 10.0f;
        _data.pv2Current = u16(pv, REG_PV2_A, REG_PV_START) / 100.0f;
        _data.pv2Power   = u16(pv, REG_PV2_W, REG_PV_START) * 10L;
    } else { ok = false; }

    // PV total (1 reg, separate block)
    uint8_t pvt[2];
    if (readBlock(REG_PV_TOTAL, 1, pvt, sz) && sz >= 2) {
        _data.pvPower = u16(pvt, REG_PV_TOTAL, REG_PV_TOTAL) * 100L;
    } else { ok = false; }

    // Battery block (14 regs)
    uint8_t bat[28];
    if (readBlock(REG_BATT_START, REG_BATT_COUNT, bat, sz) && sz >= 28) {
        _data.battVoltage  = u16(bat, REG_BATT_V,    REG_BATT_START) / 10.0f;
        _data.battCurrent  = s16(bat, REG_BATT_A,    REG_BATT_START) / 100.0f;
        _data.batteryPower = s16(bat, REG_BATT_W,    REG_BATT_START) * 10L;
        _data.battTemp     = s16(bat, REG_BATT_TEMP, REG_BATT_START);
        _data.batterySOC   = u16(bat, REG_BATT_SOC,  REG_BATT_START);
        _data.battSOH      = u16(bat, REG_BATT_SOH,  REG_BATT_START);
        _data.battCycles   = u16(bat, REG_BATT_CYC,  REG_BATT_START);
        _data.batt2Voltage = u16(bat, REG_BATT2_V,   REG_BATT_START) / 10.0f;
        _data.batt2Current = s16(bat, REG_BATT2_A,   REG_BATT_START) / 100.0f;
        _data.batt2Power   = s16(bat, REG_BATT2_W,   REG_BATT_START) * 10L;
        _data.batt2Temp    = s16(bat, REG_BATT2_TEMP,REG_BATT_START);
        _data.batt2SOC     = u16(bat, REG_BATT2_SOC, REG_BATT_START);
        _data.batt2SOH     = u16(bat, REG_BATT2_SOH, REG_BATT_START);
        _data.batt2Cycles  = u16(bat, REG_BATT2_CYC, REG_BATT_START);
    } else { ok = false; }

    // Battery totals (3 regs)
    uint8_t bt[6];
    if (readBlock(REG_BATTTOT_START, REG_BATTTOT_COUNT, bt, sz) && sz >= 6) {
        _data.battTotalPower = s16(bt, REG_BATT_TOT_W,   REG_BATTTOT_START) * 100L;
        _data.battAvgSOC     = u16(bt, REG_BATT_AVG_SOC, REG_BATTTOT_START);
        _data.battAvgSOH     = u16(bt, REG_BATT_AVG_SOH, REG_BATTTOT_START);
    } else { ok = false; }

    // Energy block (24 regs, U32 values)
    uint8_t en[48];
    if (readBlock(REG_ENERGY_START, REG_ENERGY_COUNT, en, sz) && sz >= 48) {
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
    } else { ok = false; }

    // Working mode (1 reg)
    uint8_t wm[2];
    if (readBlock(REG_WORKING_MODE, 1, wm, sz) && sz >= 2) {
        _data.workingMode = u16(wm, REG_WORKING_MODE, REG_WORKING_MODE);
    } else { ok = false; }

    _data.valid = ok;
    _error = !ok;
    return ok;
}

// ── Read serial number (once at boot) ─────────────────────────
bool Inverter::readSerialNumber() {
    uint8_t buf[16];
    uint8_t sz;
    if (!readBlock(REG_SN_START, REG_SN_COUNT, buf, sz) || sz < 16)
        return false;
    for (int i = 0; i < 16; i++) _sn[i] = (char)buf[i];
    _sn[16] = '\0';
    // Trim trailing nulls/spaces
    for (int i = 15; i >= 0; i--) {
        if (_sn[i] == '\0' || _sn[i] == ' ') _sn[i] = '\0';
        else break;
    }
    return _sn[0] != '\0';
}

// ── Send passive-mode command ──────────────────────────────────
bool Inverter::sendPassiveCommand(int32_t power) {
    int32_t minP = power;
    int32_t maxP = power;

    uint8_t payload[12] = {
        0, 0, 0, 0,
        (uint8_t)((minP >> 24) & 0xFF), (uint8_t)((minP >> 16) & 0xFF),
        (uint8_t)((minP >>  8) & 0xFF), (uint8_t)( minP        & 0xFF),
        (uint8_t)((maxP >> 24) & 0xFF), (uint8_t)((maxP >> 16) & 0xFF),
        (uint8_t)((maxP >>  8) & 0xFF), (uint8_t)( maxP        & 0xFF),
    };

    return _mb.writeMultiple(MODBUS_SLAVE_ID, REG_PASSIVE_CTRL, 6, payload, 12);
}
