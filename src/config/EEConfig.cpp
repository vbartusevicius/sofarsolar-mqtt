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
    if (changed) EEPROM.commit();   // commit rewrites a whole flash sector
}
