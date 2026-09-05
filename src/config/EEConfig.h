#ifndef SOFAR_EECONFIG_H
#define SOFAR_EECONFIG_H

#include <Arduino.h>
#include "Config.h"
#include "display/TouchCal.h"

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

    int32_t  bsaveDelta()    const { return _bsaveDelta; }
    int32_t  bsaveMaxPower() const { return _bsaveMax; }
    uint32_t idleLapseMs()   const { return (uint32_t)_idleLapseMs; }
    void setBsaveDelta(int32_t w)   { _bsaveDelta  = w; }
    void setBsaveMaxPower(int32_t w){ _bsaveMax    = w; }
    void setIdleLapseMs(uint32_t ms){ _idleLapseMs = (int32_t)ms; }

    // Control state, so a reboot (ours, yours, or the supervisor's) resumes
    // the mode that was running rather than silently falling back to auto.
    const char* mode() const           { return _mode; }
    int32_t  chargePower() const       { return _chargePower; }
    int32_t  autoLimit() const         { return _autoLimit; }
    void setMode(const char* m);
    void setChargePower(int32_t w)     { _chargePower = w; }
    void setAutoLimit(int32_t w)       { _autoLimit = w; }

    const TouchCal& touchCal() const     { return _touchCal; }
    void setTouchCal(const TouchCal& c)  { _touchCal = c; }

private:
    char _name[EE_NAME_LEN] = "Sofar";
    char _host[EE_HOST_LEN] = "";
    char _port[EE_PORT_LEN] = "1883";
    char _user[EE_USER_LEN] = "";
    char _pass[EE_PASS_LEN] = "";

    int32_t _bsaveDelta  = BSAVE_MIN_DELTA;
    int32_t _bsaveMax    = BSAVE_MAX_POWER;
    int32_t _idleLapseMs = BSAVE_IDLE_LAPSE_MS;

    TouchCal _touchCal;   // invalid until the on-screen wizard has run

    char    _mode[EE_MODE_LEN] = "auto";
    int32_t _chargePower       = 0;
    int32_t _autoLimit         = 16384;

    static void    readEE(int off, int len, char* dst);
    static bool    writeEE(int off, int len, const char* v);
    static int16_t readEE16(int off);
    static bool    writeEE16(int off, int16_t v);
    static int32_t readEE32(int off);
    static bool    writeEE32(int off, int32_t v);
};

#endif // SOFAR_EECONFIG_H
