#ifndef SOFAR_INVERTER_H
#define SOFAR_INVERTER_H

#include "Config.h"
#include "modbus/Modbus.h"
#include "inverter/InverterData.h"

class Inverter {
public:
    explicit Inverter(Modbus& mb) : _mb(mb) {}

    bool pollStep();   // cooperative 1-block-per-tick state machine
    bool readSerialNumber();
    bool sendPassiveCommand(int32_t power);
    bool sendPassiveRange(int32_t minPower, int32_t maxPower);

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
    bool track(InverterBlock b, bool ok);

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
