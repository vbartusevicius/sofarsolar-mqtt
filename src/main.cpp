#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>
#include <ESP8266mDNS.h>
#include <TaskManagerIO.h>

#include "Config.h"
#include "config/EEConfig.h"
#include "util/AppLog.h"
#include "util/HeapStats.h"
#include "modbus/Modbus.h"
#include "inverter/Inverter.h"
#include "control/BatterySaver.h"
#include "control/ModeController.h"
#include "display/Display.h"
#include "network/WiFiSetup.h"
#include "network/MqttManager.h"
#include "web/SofarWebServer.h"

static EEConfig       eeConfig;
static Modbus         modbus;
static Inverter       inverter(modbus);
static BatterySaver   bsave(inverter);
static ModeController control(inverter, bsave);
static Display        display;
static MqttManager    mqttMgr(eeConfig, inverter, bsave, control);
static SofarWebServer webServer(eeConfig, inverter, bsave, mqttMgr, control, display);

HeapStats heapStats;

void setup() {
    eeConfig.begin();
    bool configLoaded = eeConfig.load();

    bsave.setMinDelta(eeConfig.bsaveDelta());
    bsave.setMaxPower(eeConfig.bsaveMaxPower());
    bsave.setIdleLapse(eeConfig.idleLapseMs());
    inverter.setKeepaliveMs(eeConfig.keepaliveMs());

    display.begin();
    display.setTouchCal(eeConfig.touchCal());
    display.onCalibrated([](const TouchCal& c) {
        eeConfig.setTouchCal(c);
        eeConfig.save();                 // dirty-checked, so no needless commit
        appLog.add("TCH", "calibration saved");
    });
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
        display.update(inverter.data(), bsave, control, inverter.serialNumber(),
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

    if (display.pollTouch()) control.toggleBatterySaver();
    display.handleDimming();
}
