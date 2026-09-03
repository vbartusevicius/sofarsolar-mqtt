#include "network/WiFiSetup.h"
#include <WiFiManager.h>
#include "config/EEConfig.h"

static WiFiManager       wm;
static WiFiManagerParameter* pName;
static WiFiManagerParameter* pHost;
static WiFiManagerParameter* pPort;
static WiFiManagerParameter* pUser;
static WiFiManagerParameter* pPass;
static EEConfig*             _cfg = nullptr;

static void copyInto(char* dst, size_t len, const char* src) {
    strncpy(dst, src, len - 1);
    dst[len - 1] = '\0';
}

static void saveCallback() {
    if (!_cfg) return;
    copyInto(_cfg->name(),     EE_NAME_LEN, pName->getValue());
    copyInto(_cfg->mqttHost(), EE_HOST_LEN, pHost->getValue());
    copyInto(_cfg->mqttPort(), EE_PORT_LEN, pPort->getValue());
    copyInto(_cfg->mqttUser(), EE_USER_LEN, pUser->getValue());
    copyInto(_cfg->mqttPass(), EE_PASS_LEN, pPass->getValue());
    _cfg->save();
}

void setupWiFi(EEConfig& cfg) {
    _cfg = &cfg;

    pName = new WiFiManagerParameter("device", "Device name", cfg.name(),     EE_NAME_LEN);
    pHost = new WiFiManagerParameter("mqtt",   "MQTT host",   cfg.mqttHost(), EE_HOST_LEN);
    pPort = new WiFiManagerParameter("port",   "MQTT port",   cfg.mqttPort(), EE_PORT_LEN);
    pUser = new WiFiManagerParameter("user",   "MQTT user",   cfg.mqttUser(), EE_USER_LEN);
    pPass = new WiFiManagerParameter("pass",   "MQTT pass",   cfg.mqttPass(), EE_PASS_LEN);

    wm.setSaveConfigCallback(saveCallback);
    wm.setConfigPortalTimeout(180);
    wm.setConnectTimeout(30);
    wm.setDebugOutput(false);
    wm.setConfigPortalBlocking(false);

    wm.addParameter(pName);
    wm.addParameter(pHost);
    wm.addParameter(pPort);
    wm.addParameter(pUser);
    wm.addParameter(pPass);

    wm.autoConnect("SofarBatterySaver");
}

void wifiLoop() {
    wm.process();
}
