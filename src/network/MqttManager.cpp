#include "network/MqttManager.h"
#include <ArduinoJson.h>
#include "Config.h"
#include "config/EEConfig.h"
#include "inverter/Inverter.h"
#include "control/BatterySaver.h"
#include "modbus/Modbus.h"
#include "ha/Discovery.h"
#include "util/AppLog.h"

MqttManager* MqttManager::_instance = nullptr;

MqttManager::MqttManager(EEConfig& cfg, Inverter& inv,
                         BatterySaver& bs, Modbus& mb)
    : _cfg(cfg), _inv(inv), _bs(bs), _mb(mb), _mqtt(_wifiClient)
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
    if (topic.endsWith("/set/battery_save")) {
        if (msg == "on" || msg == "true" || msg == "1")  setMode("battery_saver");
        else if (_bs.isActive())                          setMode("auto");
    }
    else if (topic.endsWith("/set/charge"))  { setCharge(msg.toInt()); }
    else if (topic.endsWith("/set/standby")) { setMode("standby"); }
    else if (topic.endsWith("/set/auto"))    { setAuto(msg.toInt()); }
    else if (topic.endsWith("/set/mode"))    { setMode(msg); }
}

void MqttManager::setMode(const String& mode) {
    if (mode == "battery_saver") {
        _bs.enable();
        strncpy(_mode, "battery_saver", sizeof(_mode));
    }
    else if (mode == "standby") {
        _bs.disable();
        _inv.sendPassiveCommand(0);
        strncpy(_mode, "standby", sizeof(_mode));
    }
    else if (mode == "charge") {
        _bs.disable();
        _inv.sendPassiveCommand(_chargePower);
        strncpy(_mode, "charge", sizeof(_mode));
    }
    else { // "auto" or default
        _bs.disable();
        setAuto(_autoLimit);
    }
}

void MqttManager::setCharge(int32_t watts) {
    _bs.disable();
    _chargePower = watts;
    _inv.sendPassiveCommand(watts);
    strncpy(_mode, "charge", sizeof(_mode));
}

void MqttManager::setAuto(int32_t limit) {
    _bs.disable();
    if (limit <= 0) limit = 16384;
    _autoLimit = limit;
    uint8_t pl[12] = {
        0, 0, 0, 0,
        (uint8_t)(((-limit) >> 24) & 0xFF), (uint8_t)(((-limit) >> 16) & 0xFF),
        (uint8_t)(((-limit) >>  8) & 0xFF), (uint8_t)((-limit) & 0xFF),
        (uint8_t)((limit >> 24) & 0xFF), (uint8_t)((limit >> 16) & 0xFF),
        (uint8_t)((limit >>  8) & 0xFF), (uint8_t)(limit & 0xFF),
    };
    _mb.writeMultiple(MODBUS_SLAVE_ID, REG_PASSIVE_CTRL, 6, pl, 12);
    strncpy(_mode, "auto", sizeof(_mode));
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
        snprintf(logBuf, sizeof(logBuf), "Subscribed: %s", sub);
        appLog.add("MQTT", logBuf);
        if (!_haDiscoverySent) {
            publishHADiscovery(_mqtt, _cfg.name());
            _haDiscoverySent = true;
            appLog.add("MQTT", "HA discovery sent");
        }
    } else {
        snprintf(logBuf, sizeof(logBuf), "Connect FAILED rc=%d", _mqtt.state());
        appLog.add("MQTT", logBuf);
    }
}

void MqttManager::publish() {
    if (!_ready) return;
    if (!_mqtt.connected()) {
        char lb[48];
        snprintf(lb, sizeof(lb), "Pub skip: disconnected rc=%d", _mqtt.state());
        appLog.add("MQTT", lb);
        return;
    }
    String json = buildJSON();
    char topic[80];
    snprintf(topic, sizeof(topic), "%s/state", _cfg.name());
    bool ok = _mqtt.publish(topic, json.c_str());
    char lb[48];
    if (ok) {
        snprintf(lb, sizeof(lb), "Pub OK len=%u", (unsigned)json.length());
    } else {
        snprintf(lb, sizeof(lb), "Pub FAIL len=%u rc=%d",
                 (unsigned)json.length(), _mqtt.state());
    }
    appLog.add("MQTT", lb);
}

String MqttManager::buildJSON() {
    const InverterData& d = _inv.data();
    char fb[16];

    JsonDocument doc;
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
    doc["mode"]          = _mode;
    doc["charge_power"]  = _chargePower;
    doc["auto_limit"]    = _autoLimit;
    // Status
    doc["serial_number"] = _inv.serialNumber();
    doc["modbus_ok"]     = !_inv.hasError();
    doc["mqtt_ok"]       = _mqtt.connected();
    doc["wifi_ok"]       = WiFi.isConnected();
    doc["uptime"]        = millis();
    doc["free_heap"]     = heapStats.freeHeap;
    doc["heap_frag"]     = heapStats.frag;
    doc["max_free_block"]= heapStats.maxBlock;

    String out;
    out.reserve(measureJson(doc) + 1);
    serializeJson(doc, out);
    return out;
}
