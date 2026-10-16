#ifndef SOFAR_INVERTER_H
#define SOFAR_INVERTER_H

#include "Config.h"
#include "modbus/Modbus.h"

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
    // Mode
    uint16_t workingMode   = 0;

    uint32_t sampleSeq = 0;   // bumped on each fresh grid-power read
    bool     valid     = false;
};

class Inverter {
public:
    explicit Inverter(Modbus& mb) : _mb(mb) {}

    bool readSensors();
    bool pollStep();   // cooperative 1-block-per-tick state machine
    bool readSerialNumber();
    bool sendPassiveCommand(int32_t power);
    bool sendPassiveRange(int32_t minPower, int32_t maxPower);
    bool verifyPassiveRange(int32_t expectedMin, int32_t expectedMax);

    // Identical passive commands are re-sent at most this often. 0 = never,
    // which is correct whenever the inverter's passive timeout is disabled.
    void     setKeepaliveMs(uint32_t ms);
    uint32_t keepaliveMs() const { return _keepaliveMs; }

    // Reads 0x1184/0x1185 and derives the keep-alive from them
    bool     readPassiveTimeout();
    uint16_t passiveTimeoutS() const { return _passiveTimeoutS; }
    uint16_t timeoutAction()   const { return _timeoutAction; }

    bool hasError() const              { return _error; }
    bool isPassiveMode() const         { return _data.valid && _data.workingMode == WORK_MODE_PASSIVE; }
    const InverterData& data() const   { return _data; }
    const char* serialNumber() const   { return _sn; }

private:
    Modbus&       _mb;
    InverterData  _data;
    bool          _error = true;
    char          _sn[17] = "";

    // Passive-command write cache (limits inverter flash wear)
    bool          _cmdCacheValid = false;
    int32_t       _lastSentMin   = 0;
    int32_t       _lastSentMax   = 0;
    unsigned long _lastSentAt    = 0;
    uint32_t      _keepaliveMs   = PASSIVE_KEEPALIVE_MS;
    uint16_t      _passiveTimeoutS = 0;   // 0x1184, 0 = disabled
    uint16_t      _timeoutAction   = 0;   // 0x1185, 0 = force standby
    uint8_t       _slowTick        = 0;   // fast-poll counter for slow tier
    uint8_t       _pollStep        = 0;
    bool          _slowCycle       = true;
    static constexpr uint8_t SLOW_POLL_DIV = 6;   // 6 fast cycles

    bool readGridBlock();
    bool readPvBlock();
    bool readBattBlock();
    bool readSysBlock();
    bool readBattTotBlock();
    bool readEnergyBlock();
    bool readWorkModeBlock();

    bool readBlock(uint16_t start, uint8_t count, uint8_t* buf, uint8_t cap,
                   uint8_t& sz);

    // Capacity is taken from the array type, so a buffer can never be resized
    // without the bound following it.
    template <uint8_t N>
    bool readBlock(uint16_t start, uint8_t count, uint8_t (&buf)[N],
                   uint8_t& sz) {
        return readBlock(start, count, buf, N, sz);
    }

    static uint16_t u16(const uint8_t* b, uint16_t reg, uint16_t base);
    static int16_t  s16(const uint8_t* b, uint16_t reg, uint16_t base);
    static uint32_t u32(const uint8_t* b, uint16_t reg, uint16_t base);
};

#endif
