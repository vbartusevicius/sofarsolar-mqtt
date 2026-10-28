#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>
#include <ESP8266mDNS.h>
#include <TaskManagerIO.h>
#include <Ticker.h>

#include "Config.h"
#include "config/EEConfig.h"
#include "util/AppLog.h"
#include "util/HeapStats.h"
#include "util/Health.h"
#include "util/LoopWatchdog.h"
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
static ModeController control(inverter, bsave, eeConfig);
static Display        display;
static MqttManager    mqttMgr(eeConfig, inverter, bsave, control);
static SofarWebServer webServer(eeConfig, inverter, bsave, mqttMgr, control, display);

HeapStats heapStats;
static HealthState health;

static Ticker            loopWd;
static volatile uint32_t loopBeatAt = 0;
static volatile uint8_t  loopStage  = STAGE_LOOP;
#define STAGE(s) (loopStage = (s))

// Timer context: only RTC writes and a hard reset, nothing that can block.
static void loopWdTick() {
    LoopWdRecord r;
    bool stalled = loopStalled(millis(), loopBeatAt, LOOP_STALL_MS);
    loopWdSeal(r, loopStage, stalled, millis() / 1000);
    ESP.rtcUserMemoryWrite(LOOP_WD_RTC_OFS, (uint32_t*)&r, sizeof(r));
    if (stalled) ESP.reset();
}

static void reportPreviousStall() {
    LoopWdRecord r;
    if (!ESP.rtcUserMemoryRead(LOOP_WD_RTC_OFS, (uint32_t*)&r, sizeof(r))) return;
    if (!loopWdValid(r)) return;
    uint32_t reason = ESP.getResetInfoPtr()->reason;
    bool crashed = reason == REASON_WDT_RST || reason == REASON_EXCEPTION_RST ||
                   reason == REASON_SOFT_WDT_RST;
    if (r.stalled || crashed) {
        char lb[72];
        snprintf(lb, sizeof(lb), "%s in '%s' at uptime %lus",
                 r.stalled ? "loop stalled" : "crashed",
                 loopStageName(r.stage), (unsigned long)r.uptimeS);
        appLog.add("SYS", lb);
    }
    r.magic = 0;
    ESP.rtcUserMemoryWrite(LOOP_WD_RTC_OFS, (uint32_t*)&r, sizeof(r));
}

void setup() {
    // First line in the log after any restart: if the device ever does reboot
    // unattended, this says why (watchdog, exception, power, or our own call).
    appLog.add("SYS", ESP.getResetReason().c_str());
    reportPreviousStall();

    eeConfig.begin();
    bool configLoaded = eeConfig.load();

    bsave.setMinDelta(eeConfig.bsaveDelta());
    bsave.setMaxPower(eeConfig.bsaveMaxPower());
    bsave.setIdleLapse(eeConfig.idleLapseMs());

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
    inverter.readPassiveTimeout();   // decides whether keep-alives are needed

    if (!configLoaded) {
        if (inverter.serialNumber()[0] != '\0') {
            snprintf(eeConfig.name(), EE_NAME_LEN, "sofar_%s", inverter.serialNumber());
        } else {
            snprintf(eeConfig.name(), EE_NAME_LEN, "sofar_%06x", (unsigned int)(ESP.getChipId() & 0xFFFFFF));
        }
    }

    control.restore();   // resume the mode that was running before the reboot

    display.showSplash("WiFi Setup", "SofarBatterySaver");
    setupWiFi(eeConfig);

    ArduinoOTA.setHostname(eeConfig.name());
    ArduinoOTA.begin();
    webServer.begin();
    MDNS.begin(eeConfig.name());

    taskManager.scheduleFixedRate(INTERVAL_POLL_STEP,  []() {
        STAGE(STAGE_POLL);
        inverter.pollStep();
        STAGE(STAGE_BSAVE);
        bsave.update();
        control.reconcile();
    });
    taskManager.scheduleFixedRate(INTERVAL_BSAVE,      []() {
        STAGE(STAGE_BSAVE);
        bsave.update();
        control.reconcile();
    });
    taskManager.scheduleFixedRate(INTERVAL_MQTT_PUB,   []() {
        STAGE(STAGE_MQTT_PUB);
        mqttMgr.publish();
    });
    taskManager.scheduleFixedRate(INTERVAL_MQTT_RETRY, []() {
        if (inverter.hasError()) return;
        // Connecting with no WiFi means a DNS lookup and a TCP connect that
        // can only time out, stalling the loop for seconds each retry.
        if (!WiFi.isConnected()) return;
        STAGE(STAGE_MQTT_CONN);
        if (!mqttMgr.ready()) mqttMgr.begin();
        mqttMgr.connect();
    });
    taskManager.scheduleFixedRate(INTERVAL_DISPLAY,    []() {
        STAGE(STAGE_DISPLAY);
        display.update(inverter.data(), bsave, control, inverter.serialNumber(),
                       WiFi.isConnected(), !inverter.hasError(), mqttMgr.connected());
    });
    taskManager.scheduleFixedRate(1000, []() { heapStats.update(); });
    // Re-checked hourly: the timeout can be changed from the inverter's panel
    // or another Modbus client while we are running.
    taskManager.scheduleFixedRate(3600000, []() {
        STAGE(STAGE_PASSIVE_TMO);
        if (!inverter.hasError()) inverter.readPassiveTimeout();
    });
    taskManager.scheduleFixedRate(60000, []() {
        STAGE(STAGE_HEALTH);
        char lb[80];
        snprintf(lb, sizeof(lb), "heap=%u blk=%u min=%u frag=%u%%",
                 (unsigned)heapStats.freeHeap, (unsigned)heapStats.maxBlock,
                 (unsigned)heapStats.minHeapSeen, (unsigned)heapStats.frag);
        appLog.add("SYS", lb);

        switch (healthEvaluate(health, WiFi.isConnected(), millis(),
                               heapStats.freeHeap, heapStats.maxBlock)) {
        case HEALTH_WIFI_RETRY:
            appLog.add("SYS", "WiFi down 2 min: forcing reconnect");
            WiFi.reconnect();
            break;
        case HEALTH_REBOOT_WIFI:
            appLog.add("SYS", "WiFi down 30 min: rebooting");
            delay(100);
            ESP.restart();
            break;
        case HEALTH_REBOOT_HEAP:
            snprintf(lb, sizeof(lb), "heap exhausted (%u/%u) for 5 min: rebooting",
                     (unsigned)heapStats.freeHeap, (unsigned)heapStats.maxBlock);
            appLog.add("SYS", lb);
            delay(100);
            ESP.restart();
            break;
        case HEALTH_OK:
            break;
        }
    });

    loopBeatAt = millis();
    loopWd.attach_ms(1000, loopWdTick);
}

void loop() {
    loopBeatAt = millis();
    STAGE(STAGE_LOOP);
    taskManager.runLoop();
    STAGE(STAGE_WIFI);
    wifiLoop();
    STAGE(STAGE_OTA);
    ArduinoOTA.handle();
    STAGE(STAGE_MDNS);
    MDNS.update();
    STAGE(STAGE_WEB);
    webServer.handleClient();
    STAGE(STAGE_MQTT_LOOP);
    mqttMgr.loop();

    STAGE(STAGE_TOUCH);
    if (display.pollTouch()) control.toggleBatterySaver();
    display.handleDimming();
}
