#include "network/MqttManager.h"
#include <ArduinoJson.h>
#include "util/HeapStats.h"
#include "config/EEConfig.h"
#include "inverter/Inverter.h"
#include "control/BatterySaver.h"
#include "control/ModeController.h"
#include "ha/Discovery.h"
#include "util/AppLog.h"
#include "Version.h"

MqttManager* MqttManager::_instance = nullptr;

MqttManager::MqttManager(EEConfig& cfg, Inverter& inv,
                         BatterySaver& bs, ModeController& ctrl)
    : _cfg(cfg), _inv(inv), _bs(bs), _ctrl(ctrl), _mqtt(_wifiClient)
{
    _instance = this;
}

void MqttManager::callbackTrampoline(char* topic, byte* payload,
                                     unsigned int len)
{
    if (!_instance) return;
    String msg;
    msg.reserve(len);
    for (unsigned int i = 0; i < len; i++) msg += (char)payload[i];
    _instance->handleMessage(String(topic), msg);
}

void MqttManager::handleMessage(const String& topic, const String& msg) {
    if (topic.endsWith("/ping")) {
        uint32_t seq = (uint32_t)msg.toInt();
        if ((uint32_t)(seq - _echoSeq) < 0x80000000UL) _echoSeq = seq;
        return;
    }
    if (topic.endsWith("/set/battery_save")) {
        if (msg == "on" || msg == "true" || msg == "1")  _ctrl.setMode("battery_saver");
        else if (_bs.isActive())                          _ctrl.setMode("auto");
    }
    else if (topic.endsWith("/set/charge"))  { _ctrl.setCharge(msg.toInt()); }
    else if (topic.endsWith("/set/standby")) { _ctrl.setMode("standby"); }
    else if (topic.endsWith("/set/auto"))    { _ctrl.setAuto(msg.toInt()); }
    else if (topic.endsWith("/set/mode"))    { _ctrl.setMode(msg.c_str()); }
}

void MqttManager::begin() {
    _wifiClient.setTimeout(5000);
    _mqtt.setSocketTimeout(5);
    _mqtt.setBufferSize(2048);
    _ready = true;
    appLog.add("MQTT", "begin buf=2048");
    connect();
}

void MqttManager::connect() {
    if (_mqtt.connected()) return;

    int port = atoi(_cfg.mqttPort());

    char clientId[80];
    snprintf(clientId, sizeof(clientId), "%s-%x", _cfg.name(), (unsigned)ESP.getChipId());
    char willTopic[80];
    snprintf(willTopic, sizeof(willTopic), "%s/status", _cfg.name());

    char logBuf[192];
    snprintf(logBuf, sizeof(logBuf), "Connecting to %s:%d client=%s",
             _cfg.mqttHost(), port, clientId);
    appLog.add("MQTT", logBuf);

    _mqtt.setServer(_cfg.mqttHost(), port);
    _mqtt.setCallback(callbackTrampoline);

    if (_mqtt.connect(clientId, _cfg.mqttUser(), _cfg.mqttPass(),
                      willTopic, 0, true, "offline"))
    {
        appLog.add("MQTT", "Connected OK");
        _mqtt.publish(willTopic, "online", true);
        char sub[80];
        snprintf(sub, sizeof(sub), "%s/set/#", _cfg.name());
        _mqtt.subscribe(sub);
        snprintf(sub, sizeof(sub), "%s/ping", _cfg.name());
        _mqtt.subscribe(sub);
        appLog.add("MQTT", "Subscribed");
        _pingSeq = _echoSeq = 0;
        _lastPingAt = millis();
        // retained discovery configs die with a stateless broker restart
        publishHADiscovery(_mqtt, _cfg.name());
        appLog.add("MQTT", "HA discovery sent");
    } else {
        snprintf(logBuf, sizeof(logBuf), "Connect FAILED rc=%d", _mqtt.state());
        appLog.add("MQTT", logBuf);
    }
}

// PubSubClient only inspects the socket; behind a proxy the socket can stay
// open while the session is dead. Only a broker round-trip proves the link.
void MqttManager::sendEchoPing() {
    _pingSeq++;
    char topic[80], payload[12];
    snprintf(topic, sizeof(topic), "%s/ping", _cfg.name());
    snprintf(payload, sizeof(payload), "%u", (unsigned)_pingSeq);
    bool ok = _mqtt.publish(topic, payload);
    char lb[48];
    snprintf(lb, sizeof(lb), "Echo ping #%u %s (echo at #%u)",
             (unsigned)_pingSeq, ok ? "sent" : "FAILED", (unsigned)_echoSeq);
    appLog.add("MQTT", lb);
}

void MqttManager::checkLiveness() {
    unsigned long now = millis();
    if (now - _lastPingAt < MQTT_ECHO_PING_MS) return;
    _lastPingAt = now;

    if (_pingSeq > 0 &&
        (uint32_t)(_pingSeq - _echoSeq) >= MQTT_ECHO_MISS_TOLERANCE) {
        forceReconnect("stale link (no echo)");
        return;
    }
    sendEchoPing();
}

void MqttManager::forceReconnect(const char* reason) {
    char lb[80];
    snprintf(lb, sizeof(lb), "Forcing reconnect: %s rc=%d", reason, _mqtt.state());
    appLog.add("MQTT", lb);
    _mqtt.disconnect();          // retry task reconnects
    _pingSeq = _echoSeq = 0;
}

void MqttManager::loop() {
    if (!_ready) return;
    _mqtt.loop();
    if (_mqtt.connected()) checkLiveness();
}

void MqttManager::publish() {
    if (!_ready) return;
    if (!_mqtt.connected()) {
        char lb[48];
        snprintf(lb, sizeof(lb), "Pub skip: disconnected rc=%d", _mqtt.state());
        appLog.add("MQTT", lb);
        return;
    }
    JsonDocument doc;
    fillState(doc);
    char topic[80];
    snprintf(topic, sizeof(topic), "%s/state", _cfg.name());

    // stream to the socket, no intermediate String
    size_t len = measureJson(doc);
    bool ok = _mqtt.beginPublish(topic, len, false);
    if (ok) {
        serializeJson(doc, _mqtt);
        ok = _mqtt.endPublish();
    }

    char lb[48];
    snprintf(lb, sizeof(lb), "Pub %s len=%u%s", ok ? "OK" : "FAIL",
             (unsigned)len, ok ? "" : " (socket)");
    appLog.add("MQTT", lb);
}

void MqttManager::fillState(JsonDocument& doc) {
    const InverterData& d = _inv.data();
    char fb[16];

    // System
    doc["run_state"]      = d.runState;
    doc["inverter_temp"]  = d.inverterTemp;
    doc["heatsink_temp"]  = d.heatsinkTemp;
    // Grid
    doc["grid_freq"]      = serialized(dtostrf(d.gridFreq, 0, 2, fb));
    doc["inverter_power"] = d.inverterPower;
    doc["grid_power"]     = d.gridPower;
    doc["grid_voltage"]   = serialized(dtostrf(d.gridVoltage, 0, 1, fb));
    doc["load_power"]     = d.loadPower;
    // PV
    doc["pv1_voltage"]    = serialized(dtostrf(d.pv1Voltage, 0, 1, fb));
    doc["pv1_current"]    = serialized(dtostrf(d.pv1Current, 0, 2, fb));
    doc["pv1_power"]      = d.pv1Power;
    doc["pv2_voltage"]    = serialized(dtostrf(d.pv2Voltage, 0, 1, fb));
    doc["pv2_current"]    = serialized(dtostrf(d.pv2Current, 0, 2, fb));
    doc["pv2_power"]      = d.pv2Power;
    doc["pv_total"]       = d.pvPower;
    // Battery 1
    doc["batt_voltage"]   = serialized(dtostrf(d.battVoltage, 0, 1, fb));
    doc["batt_current"]   = serialized(dtostrf(d.battCurrent, 0, 2, fb));
    doc["batt_power"]     = d.batteryPower;
    doc["batt_temp"]      = d.battTemp;
    doc["batt_soc"]       = d.batterySOC;
    doc["batt_soh"]       = d.battSOH;
    doc["batt_cycles"]    = d.battCycles;
    // Battery 2
    doc["batt2_voltage"]  = serialized(dtostrf(d.batt2Voltage, 0, 1, fb));
    doc["batt2_current"]  = serialized(dtostrf(d.batt2Current, 0, 2, fb));
    doc["batt2_power"]    = d.batt2Power;
    doc["batt2_temp"]     = d.batt2Temp;
    doc["batt2_soc"]      = d.batt2SOC;
    doc["batt2_soh"]      = d.batt2SOH;
    doc["batt2_cycles"]   = d.batt2Cycles;
    // Battery totals
    doc["batt_total_power"] = d.battTotalPower;
    doc["batt_avg_soc"]   = d.battAvgSOC;
    doc["batt_avg_soh"]   = d.battAvgSOH;
    // Energy
    doc["today_gen"]      = serialized(dtostrf(d.todayGeneration, 0, 2, fb));
    doc["total_gen"]      = serialized(dtostrf(d.totalGeneration, 0, 1, fb));
    doc["today_use"]      = serialized(dtostrf(d.todayConsumption, 0, 2, fb));
    doc["total_use"]      = serialized(dtostrf(d.totalConsumption, 0, 1, fb));
    doc["today_imp"]      = serialized(dtostrf(d.todayImport, 0, 2, fb));
    doc["total_imp"]      = serialized(dtostrf(d.totalImport, 0, 1, fb));
    doc["today_exp"]      = serialized(dtostrf(d.todayExport, 0, 2, fb));
    doc["total_exp"]      = serialized(dtostrf(d.totalExport, 0, 1, fb));
    doc["today_chg"]      = serialized(dtostrf(d.todayCharged, 0, 2, fb));
    doc["total_chg"]      = serialized(dtostrf(d.totalCharged, 0, 1, fb));
    doc["today_dis"]      = serialized(dtostrf(d.todayDischarged, 0, 2, fb));
    doc["total_dis"]      = serialized(dtostrf(d.totalDischarged, 0, 1, fb));
    // Mode & control
    doc["working_mode"]        = d.workingMode;
    doc["battery_save"]        = _bs.isActive();
    doc["battery_save_target"] = _bs.targetPower();
    // Control state
    doc["mode"]          = _ctrl.currentMode();
    doc["charge_power"]  = _ctrl.chargePower();
    doc["auto_limit"]    = _ctrl.autoLimit();
    // Status
    doc["serial_number"] = _inv.serialNumber();
    doc["firmware"]      = FW_VERSION;
    doc["modbus_ok"]     = !_inv.hasError();
    doc["mqtt_ok"]       = _mqtt.connected();
    doc["wifi_ok"]       = WiFi.isConnected();
    doc["uptime"]        = millis();
    doc["free_heap"]     = heapStats.freeHeap;
    doc["heap_frag"]     = heapStats.frag;
    doc["max_free_block"]= heapStats.maxBlock;
    // Passive-mode write policy, read from the inverter (0x1184/0x1185)
    doc["passive_timeout_s"] = _inv.passiveTimeoutS();
    doc["timeout_action"]    = _inv.timeoutAction();
    doc["keepalive_s"]       = _inv.keepaliveMs() / 1000;
}
