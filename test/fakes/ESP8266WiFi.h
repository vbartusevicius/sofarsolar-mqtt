#ifndef FAKE_ESP8266WIFI_H
#define FAKE_ESP8266WIFI_H

#include <cstring>

// Fake WiFi singleton: only hostname() is used by EEConfig.
class FakeWiFiClass {
public:
    char hostnameBuf[64] = "";
    void hostname(const char* h) {
        strncpy(hostnameBuf, h, sizeof(hostnameBuf) - 1);
        hostnameBuf[sizeof(hostnameBuf) - 1] = '\0';
    }
};

inline FakeWiFiClass WiFi;

#endif
