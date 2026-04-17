#include "network/WiFiSetup.h"
#include <WiFiManager.h>
#include "config/EEConfig.h"

static bool shouldSave = false;
static void saveCallback() { shouldSave = true; }

void setupWiFi(EEConfig& cfg) {
    WiFiManagerParameter pName("device", "Device name", cfg.name(),     EE_NAME_LEN);
    WiFiManagerParameter pHost("mqtt",   "MQTT host",   cfg.mqttHost(), EE_HOST_LEN);
    WiFiManagerParameter pPort("port",   "MQTT port",   cfg.mqttPort(), EE_PORT_LEN);
    WiFiManagerParameter pUser("user",   "MQTT user",   cfg.mqttUser(), EE_USER_LEN);
    WiFiManagerParameter pPass("pass",   "MQTT pass",   cfg.mqttPass(), EE_PASS_LEN);

    WiFiManager wm;
    wm.setSaveConfigCallback(saveCallback);
    wm.setConfigPortalTimeout(180);
    wm.setConnectTimeout(30);
    wm.setDebugOutput(false);

    wm.addParameter(&pName);
    wm.addParameter(&pHost);
    wm.addParameter(&pPort);
    wm.addParameter(&pUser);
    wm.addParameter(&pPass);

    if (!wm.autoConnect("SofarBatterySaver")) {
        ESP.reset();
    }

    strcpy(cfg.name(),     pName.getValue());
    strcpy(cfg.mqttHost(), pHost.getValue());
    strcpy(cfg.mqttPort(), pPort.getValue());
    strcpy(cfg.mqttUser(), pUser.getValue());
    strcpy(cfg.mqttPass(), pPass.getValue());

    if (shouldSave) cfg.save();
}
