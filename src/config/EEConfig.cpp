#include "config/EEConfig.h"
#include <EEPROM.h>
#include <ESP8266WiFi.h>

void EEConfig::begin() {
    EEPROM.begin(EE_SIZE);
}

String EEConfig::readEE(int off, int len) {
    String s;
    s.reserve(len);
    for (int i = 0; i < len; i++) s += (char)EEPROM.read(off + i);
    return s;
}

void EEConfig::writeEE(int off, int len, const char* v) {
    int vlen = strlen(v);
    for (int i = 0; i < len; i++)
        EEPROM.write(off + i, i < vlen ? v[i] : 0);
}

bool EEConfig::load() {
    if ((char)EEPROM.read(EE_MAGIC) != '1') return false;
    readEE(EE_NAME, EE_NAME_LEN).toCharArray(_name, EE_NAME_LEN);
    readEE(EE_HOST, EE_HOST_LEN).toCharArray(_host, EE_HOST_LEN);
    readEE(EE_PORT, EE_PORT_LEN).toCharArray(_port, EE_PORT_LEN);
    readEE(EE_USER, EE_USER_LEN).toCharArray(_user, EE_USER_LEN);
    readEE(EE_PASS, EE_PASS_LEN).toCharArray(_pass, EE_PASS_LEN);
    WiFi.hostname(_name);
    return true;
}

void EEConfig::save() {
    writeEE(EE_MAGIC, 1, "1");
    writeEE(EE_NAME, EE_NAME_LEN, _name);
    writeEE(EE_HOST, EE_HOST_LEN, _host);
    writeEE(EE_PORT, EE_PORT_LEN, _port);
    writeEE(EE_USER, EE_USER_LEN, _user);
    writeEE(EE_PASS, EE_PASS_LEN, _pass);
    EEPROM.write(199, 2);   // inverterModel = HYDV2
    EEPROM.write(200, 1);   // tftModel = true
    EEPROM.commit();
}
