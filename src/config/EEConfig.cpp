#include "config/EEConfig.h"
#include <EEPROM.h>
#include <ESP8266WiFi.h>

void EEConfig::begin() {
    EEPROM.begin(EE_SIZE);
}

void EEConfig::readEE(int off, int len, char* dst) {
    int i = 0;
    for (; i < len - 1; i++) {
        char c = (char)EEPROM.read(off + i);
        if (c == '\0' || c == (char)0xFF) break;   // NUL or erased flash
        dst[i] = c;
    }
    dst[i] = '\0';
}

// Writes only bytes that actually differ; returns true if anything changed.
bool EEConfig::writeEE(int off, int len, const char* v) {
    bool changed = false;
    int vlen = strlen(v);
    for (int i = 0; i < len; i++) {
        uint8_t b = i < vlen ? (uint8_t)v[i] : 0;
        if (EEPROM.read(off + i) != b) { EEPROM.write(off + i, b); changed = true; }
    }
    return changed;
}

int16_t EEConfig::readEE16(int off) {
    uint16_t v = (uint16_t)EEPROM.read(off) | ((uint16_t)EEPROM.read(off + 1) << 8);
    return (int16_t)v;
}

bool EEConfig::writeEE16(int off, int16_t v) {
    bool changed = false;
    uint16_t u = (uint16_t)v;
    for (int i = 0; i < 2; i++) {
        uint8_t b = (uint8_t)(u >> (8 * i));
        if (EEPROM.read(off + i) != b) { EEPROM.write(off + i, b); changed = true; }
    }
    return changed;
}

int32_t EEConfig::readEE32(int off) {
    uint32_t v = 0;
    for (int i = 0; i < 4; i++) v |= (uint32_t)EEPROM.read(off + i) << (8 * i);
    return (int32_t)v;
}

bool EEConfig::writeEE32(int off, int32_t v) {
    bool changed = false;
    uint32_t u = (uint32_t)v;
    for (int i = 0; i < 4; i++) {
        uint8_t b = (uint8_t)(u >> (8 * i));
        if (EEPROM.read(off + i) != b) { EEPROM.write(off + i, b); changed = true; }
    }
    return changed;
}

bool EEConfig::load() {
    if ((char)EEPROM.read(EE_MAGIC) != '1') return false;
    readEE(EE_NAME, EE_NAME_LEN, _name);
    readEE(EE_HOST, EE_HOST_LEN, _host);
    readEE(EE_PORT, EE_PORT_LEN, _port);
    readEE(EE_USER, EE_USER_LEN, _user);
    readEE(EE_PASS, EE_PASS_LEN, _pass);
    // -1 = erased flash (field never written) → keep compiled default
    int32_t v;
    if ((v = readEE32(EE_BSAVE_DELTA))   != -1) _bsaveDelta  = v;
    if ((v = readEE32(EE_BSAVE_MAX))     != -1) _bsaveMax    = v;
    if ((v = readEE32(EE_KEEPALIVE_MS))  != -1) _keepaliveMs = v;
    if ((v = readEE32(EE_IDLE_LAPSE_MS)) != -1) _idleLapseMs = v;
    if (EEPROM.read(EE_TOUCH_CAL) == 1) {         // 0xFF = never calibrated
        _touchCal.valid = true;
        _touchCal.swap  = EEPROM.read(EE_TOUCH_CAL + 1) != 0;
        _touchCal.xLo   = readEE16(EE_TOUCH_CAL + 2);
        _touchCal.xHi   = readEE16(EE_TOUCH_CAL + 4);
        _touchCal.yLo   = readEE16(EE_TOUCH_CAL + 6);
        _touchCal.yHi   = readEE16(EE_TOUCH_CAL + 8);
    }
    WiFi.hostname(_name);
    return true;
}

void EEConfig::save() {
    bool changed = false;
    changed |= writeEE(EE_MAGIC, 1, "1");
    changed |= writeEE(EE_NAME, EE_NAME_LEN, _name);
    changed |= writeEE(EE_HOST, EE_HOST_LEN, _host);
    changed |= writeEE(EE_PORT, EE_PORT_LEN, _port);
    changed |= writeEE(EE_USER, EE_USER_LEN, _user);
    changed |= writeEE(EE_PASS, EE_PASS_LEN, _pass);
    if (EEPROM.read(EE_INVERTER_MODEL) != 2) { EEPROM.write(EE_INVERTER_MODEL, 2); changed = true; }
    if (EEPROM.read(EE_TFT_MODEL)      != 1) { EEPROM.write(EE_TFT_MODEL, 1);      changed = true; }
    changed |= writeEE32(EE_BSAVE_DELTA,   _bsaveDelta);
    changed |= writeEE32(EE_BSAVE_MAX,     _bsaveMax);
    changed |= writeEE32(EE_KEEPALIVE_MS,  _keepaliveMs);
    changed |= writeEE32(EE_IDLE_LAPSE_MS, _idleLapseMs);
    uint8_t calFlag = _touchCal.valid ? 1 : 0;
    uint8_t calSwap  = _touchCal.swap  ? 1 : 0;
    if (EEPROM.read(EE_TOUCH_CAL)     != calFlag) { EEPROM.write(EE_TOUCH_CAL, calFlag);        changed = true; }
    if (EEPROM.read(EE_TOUCH_CAL + 1) != calSwap) { EEPROM.write(EE_TOUCH_CAL + 1, calSwap);    changed = true; }
    changed |= writeEE16(EE_TOUCH_CAL + 2, _touchCal.xLo);
    changed |= writeEE16(EE_TOUCH_CAL + 4, _touchCal.xHi);
    changed |= writeEE16(EE_TOUCH_CAL + 6, _touchCal.yLo);
    changed |= writeEE16(EE_TOUCH_CAL + 8, _touchCal.yHi);
    if (changed) EEPROM.commit();   // commit rewrites a whole flash sector
}
