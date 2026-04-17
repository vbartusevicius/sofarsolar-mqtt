#ifndef SOFAR_INVERTER_H
#define SOFAR_INVERTER_H

#include "modbus/Modbus.h"

struct InverterData {
    // System
    uint16_t runState      = 0;
    int16_t  inverterTemp  = 0;      // °C
    int16_t  heatsinkTemp  = 0;      // °C
    // Grid
    float    gridFreq      = 0;      // Hz
    int32_t  inverterPower = 0;      // W
    int32_t  gridPower     = 0;      // W  (+export / −import)
    float    gridVoltage   = 0;      // V
    int32_t  loadPower     = 0;      // W
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
    // Mode
    uint16_t workingMode   = 0;

    bool valid = false;
};

class Inverter {
public:
    explicit Inverter(Modbus& mb) : _mb(mb) {}

    bool readSensors();
    bool sendPassiveCommand(int32_t power);

    bool hasError() const              { return _error; }
    const InverterData& data() const   { return _data; }

private:
    Modbus&       _mb;
    InverterData  _data;
    bool          _error = true;

    bool readBlock(uint16_t start, uint8_t count, uint8_t* buf, uint8_t& sz);

    static uint16_t u16(const uint8_t* b, uint16_t reg, uint16_t base);
    static int16_t  s16(const uint8_t* b, uint16_t reg, uint16_t base);
    static uint32_t u32(const uint8_t* b, uint16_t reg, uint16_t base);
};

#endif // SOFAR_INVERTER_H
