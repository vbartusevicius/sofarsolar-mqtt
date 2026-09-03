#ifndef SOFAR_EECONFIG_H
#define SOFAR_EECONFIG_H

#include <Arduino.h>
#include "Config.h"

class EEConfig {
public:
    void begin();
    bool load();
    void save();

    char*       name()     { return _name; }
    char*       mqttHost() { return _host; }
    char*       mqttPort() { return _port; }
    char*       mqttUser() { return _user; }
    char*       mqttPass() { return _pass; }

    const char* name()     const { return _name; }
    const char* mqttHost() const { return _host; }
    const char* mqttPort() const { return _port; }
    const char* mqttUser() const { return _user; }
    const char* mqttPass() const { return _pass; }

    // Battery-saver tuning (all values are clamped by BatterySaver/Inverter)
    int32_t  bsaveDelta()    const { return _bsaveDelta; }
    int32_t  bsaveMaxPower() const { return _bsaveMax; }
    uint32_t keepaliveMs()   const { return (uint32_t)_keepaliveMs; }
    uint32_t idleLapseMs()   const { return (uint32_t)_idleLapseMs; }
    uint32_t otaCheckS()     const { return (uint32_t)_otaCheckS; }
    void setBsaveDelta(int32_t w)   { _bsaveDelta  = w; }
    void setBsaveMaxPower(int32_t w){ _bsaveMax    = w; }
    void setKeepaliveMs(uint32_t ms){ _keepaliveMs = (int32_t)ms; }
    void setIdleLapseMs(uint32_t ms){ _idleLapseMs = (int32_t)ms; }
    void setOtaCheckS(uint32_t s)   { _otaCheckS   = (int32_t)s; }

private:
    char _name[EE_NAME_LEN] = "Sofar";
    char _host[EE_HOST_LEN] = "";
    char _port[EE_PORT_LEN] = "1883";
    char _user[EE_USER_LEN] = "";
    char _pass[EE_PASS_LEN] = "";

    int32_t _bsaveDelta  = BSAVE_MIN_DELTA;
    int32_t _bsaveMax    = BSAVE_MAX_POWER;
    int32_t _keepaliveMs = PASSIVE_KEEPALIVE_MS;
    int32_t _idleLapseMs = BSAVE_IDLE_LAPSE_MS;
    int32_t _otaCheckS   = OTA_CHECK_INTERVAL_S;

    static void    readEE(int off, int len, char* dst);
    static bool    writeEE(int off, int len, const char* v);
    static int32_t readEE32(int off);
    static bool    writeEE32(int off, int32_t v);
};

#endif // SOFAR_EECONFIG_H
