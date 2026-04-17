#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>
#include <ESP8266mDNS.h>
#include <TaskManagerIO.h>

#include "Config.h"
#include "config/EEConfig.h"
#include "modbus/Modbus.h"
#include "inverter/Inverter.h"
#include "control/BatterySaver.h"
#include "display/Display.h"
#include "network/WiFiSetup.h"
#include "network/MqttManager.h"
#include "web/SofarWebServer.h"

// ── Objects ─────────────────────────────────────────────────────
static EEConfig       eeConfig;
static Modbus         modbus;
static Inverter       inverter(modbus);
static BatterySaver   bsave(inverter);
static Display        display;
static MqttManager    mqttMgr(eeConfig, inverter, bsave, modbus);
static SofarWebServer webServer(eeConfig, bsave, mqttMgr);

// ── Arduino entry points ────────────────────────────────────────
void setup() {
    eeConfig.begin();
    bool configLoaded = eeConfig.load();

    display.begin();
    modbus.begin(MODBUS_BAUD);
    delay(500);

    // If no saved config, try to use inverter SN as default device name
    if (!configLoaded) {
        if (inverter.readSerialNumber()) {
            snprintf(eeConfig.name(), EE_NAME_LEN, "sofar_%s", inverter.serialNumber());
        } else {
            snprintf(eeConfig.name(), EE_NAME_LEN, "sofar_%06x", (unsigned int)(ESP.getChipId() & 0xFFFFFF));
        }
    }

    display.showSplash("WiFi Setup", "SofarBatterySaver");
    setupWiFi(eeConfig);

    ArduinoOTA.setHostname(eeConfig.name());
    ArduinoOTA.begin();
    webServer.begin();
    MDNS.begin(eeConfig.name());
    mqttMgr.begin();

    // Schedule periodic tasks via TaskManagerIO
    taskManager.scheduleFixedRate(INTERVAL_SENSORS,    []() { inverter.readSensors(); });
    taskManager.scheduleFixedRate(INTERVAL_BSAVE,      []() { bsave.update(); });
    taskManager.scheduleFixedRate(INTERVAL_MQTT_PUB,   []() { mqttMgr.publish(); });
    taskManager.scheduleFixedRate(INTERVAL_MQTT_RETRY, []() { mqttMgr.connect(); });
    taskManager.scheduleFixedRate(INTERVAL_DISPLAY,    []() {
        display.update(inverter.data(), bsave,
                       WiFi.isConnected(), !inverter.hasError(), mqttMgr.connected());
    });
}

void loop() {
    taskManager.runLoop();
    wifiLoop();
    ArduinoOTA.handle();
    MDNS.update();
    webServer.handleClient();
    mqttMgr.loop();

    if (display.pollTouch()) bsave.toggle();
    display.handleDimming();
}
