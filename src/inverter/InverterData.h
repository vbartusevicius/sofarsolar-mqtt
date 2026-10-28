#ifndef SOFAR_INVERTER_DATA_H
#define SOFAR_INVERTER_DATA_H

#include <stdint.h>

// Which register blocks were read successfully in their most recent attempt
enum InverterBlock : uint8_t {
    BLOCK_GRID     = 1 << 0,
    BLOCK_PV       = 1 << 1,
    BLOCK_BATT     = 1 << 2,
    BLOCK_SYS      = 1 << 3,
    BLOCK_BATTTOT  = 1 << 4,
    BLOCK_ENERGY   = 1 << 5,
    BLOCK_WORKMODE = 1 << 6,
};
#define BLOCK_FAST_MASK (BLOCK_GRID | BLOCK_PV | BLOCK_BATT)

struct InverterData {
    // System
    uint16_t runState      = 0;
    int16_t  inverterTemp  = 0;      // °C
    int16_t  heatsinkTemp  = 0;      // °C
    // Grid
    float    gridFreq          = 0;      // Hz
    int32_t  inverterPower     = 0;      // W
    int32_t  inverterSelfPower = 0;      // W  (self-consumption + conversion loss)
    int32_t  gridPower         = 0;      // W  (+export / −import)
    float    gridVoltage       = 0;      // V  (L1)
    float    gridVoltageL2     = 0;      // V  (L2)
    float    gridVoltageL3     = 0;      // V  (L3)
    int32_t  loadPower         = 0;      // W
    // PV
    float    pv1Voltage    = 0;      // V
    float    pv1Current    = 0;      // A
    int32_t  pv1Power      = 0;      // W
    float    pv2Voltage    = 0;      // V
    float    pv2Current    = 0;      // A
    int32_t  pv2Power      = 0;      // W
    int32_t  pvPower       = 0;      // W  (total)
    // Battery 1
    float    battVoltage   = 0;      // V
    float    battCurrent   = 0;      // A
    int32_t  batteryPower  = 0;      // W  (+charge / −discharge)
    int16_t  battTemp      = 0;      // °C
    uint16_t batterySOC    = 0;      // %
    uint16_t battSOH       = 0;      // %
    uint16_t battCycles    = 0;
    // Battery 2
    float    batt2Voltage  = 0;      // V
    float    batt2Current  = 0;      // A
    int32_t  batt2Power    = 0;      // W
    int16_t  batt2Temp     = 0;      // °C
    uint16_t batt2SOC      = 0;      // %
    uint16_t batt2SOH      = 0;      // %
    uint16_t batt2Cycles   = 0;
    // Battery totals
    int32_t  battTotalPower = 0;     // W
    uint16_t battAvgSOC    = 0;      // %
    uint16_t battAvgSOH    = 0;      // %
    // Energy counters (kWh)
    float todayGeneration  = 0;
    float totalGeneration  = 0;
    float todayConsumption = 0;
    float totalConsumption = 0;
    float todayImport      = 0;
    float totalImport      = 0;
    float todayExport      = 0;
    float totalExport      = 0;
    float todayCharged     = 0;
    float totalCharged     = 0;
    float todayDischarged  = 0;
    float totalDischarged  = 0;
    uint8_t selfSufficiency = 100;   // % of today's use not imported
    // Mode
    uint16_t workingMode   = 0;

    uint32_t sampleSeq = 0;   // bumped on each fresh grid-power read
    uint8_t  blockOk   = 0;   // InverterBlock bits
    bool     valid     = false;

    bool fresh() const { return (blockOk & BLOCK_FAST_MASK) == BLOCK_FAST_MASK; }
};

#endif // SOFAR_INVERTER_DATA_H
