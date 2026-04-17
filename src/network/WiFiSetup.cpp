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

static void saveCallback() {
    if (!_cfg) return;
    strcpy(_cfg->name(),     pName->getValue());
    strcpy(_cfg->mqttHost(), pHost->getValue());
    strcpy(_cfg->mqttPort(), pPort->getValue());
    strcpy(_cfg->mqttUser(), pUser->getValue());
    strcpy(_cfg->mqttPass(), pPass->getValue());
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
