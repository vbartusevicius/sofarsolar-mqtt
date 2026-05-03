#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>
#include <ESP8266mDNS.h>
#include <TaskManagerIO.h>

#include "Config.h"
#include "config/EEConfig.h"
#include "util/AppLog.h"
#include "modbus/Modbus.h"
#include "inverter/Inverter.h"
#include "control/BatterySaver.h"
#include "display/Display.h"
#include "network/WiFiSetup.h"
#include "network/MqttManager.h"
#include "web/SofarWebServer.h"

static EEConfig       eeConfig;
static Modbus         modbus;
static Inverter       inverter(modbus);
static BatterySaver   bsave(inverter);
static Display        display;
static MqttManager    mqttMgr(eeConfig, inverter, bsave, modbus);
static SofarWebServer webServer(eeConfig, inverter, bsave, mqttMgr);

HeapStats heapStats;

void setup() {
    eeConfig.begin();
    bool configLoaded = eeConfig.load();

    display.begin();
    modbus.begin(MODBUS_BAUD);
    delay(500);
    inverter.readSerialNumber();

    if (!configLoaded) {
        if (inverter.serialNumber()[0] != '\0') {
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

    taskManager.scheduleFixedRate(INTERVAL_SENSORS,    []() { inverter.readSensors(); });
    taskManager.scheduleFixedRate(INTERVAL_BSAVE,      []() { bsave.update(); });
    taskManager.scheduleFixedRate(INTERVAL_MQTT_PUB,   []() { mqttMgr.publish(); });
    taskManager.scheduleFixedRate(INTERVAL_MQTT_RETRY, []() {
        if (inverter.hasError()) return;
        if (!mqttMgr.ready()) mqttMgr.begin();
        mqttMgr.connect();
    });
    taskManager.scheduleFixedRate(INTERVAL_DISPLAY,    []() {
        display.update(inverter.data(), bsave,
                       WiFi.isConnected(), !inverter.hasError(), mqttMgr.connected());
    });
    taskManager.scheduleFixedRate(1000, []() { heapStats.update(); });
    taskManager.scheduleFixedRate(60000, []() {
        char lb[80];
        snprintf(lb, sizeof(lb), "heap=%u blk=%u min=%u frag=%u%%",
                 (unsigned)heapStats.freeHeap, (unsigned)heapStats.maxBlock,
                 (unsigned)heapStats.minHeapSeen, (unsigned)heapStats.frag);
        appLog.add("SYS", lb);
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
