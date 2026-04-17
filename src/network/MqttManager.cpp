#include "network/MqttManager.h"
#include <ArduinoJson.h>
#include "Config.h"
#include "config/EEConfig.h"
#include "inverter/Inverter.h"
#include "control/BatterySaver.h"
#include "modbus/Modbus.h"
#include "ha/Discovery.h"

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
        if (msg == "on" || msg == "true" || msg == "1")  _bs.enable();
        else                                              _bs.disable();
    }
    else if (topic.endsWith("/set/charge")) {
        _bs.disable();
        _inv.sendPassiveCommand(msg.toInt());
    }
    else if (topic.endsWith("/set/standby")) {
        _bs.disable();
        _inv.sendPassiveCommand(0);
    }
    else if (topic.endsWith("/set/auto")) {
        _bs.disable();
        int32_t limit = msg.toInt();
        if (limit <= 0) limit = 16384;
        uint8_t pl[12] = {
            0, 0, 0, 0,
            (uint8_t)(((-limit) >> 24) & 0xFF), (uint8_t)(((-limit) >> 16) & 0xFF),
            (uint8_t)(((-limit) >>  8) & 0xFF), (uint8_t)((-limit) & 0xFF),
            (uint8_t)((limit >> 24) & 0xFF), (uint8_t)((limit >> 16) & 0xFF),
            (uint8_t)((limit >>  8) & 0xFF), (uint8_t)(limit & 0xFF),
        };
        _mb.writeMultiple(MODBUS_SLAVE_ID, REG_PASSIVE_CTRL, 6, pl, 12);
    }
}

void MqttManager::begin() {
    _mqtt.setBufferSize(1024);
    connect();
}

void MqttManager::connect() {
    if (_mqtt.connected()) return;

    String clientId  = String(_cfg.name()) + "-" + String(ESP.getChipId(), HEX);
    String willTopic = String(_cfg.name()) + "/status";

    _mqtt.setServer(_cfg.mqttHost(), atoi(_cfg.mqttPort()));
    _mqtt.setCallback(callbackTrampoline);

    if (_mqtt.connect(clientId.c_str(), _cfg.mqttUser(), _cfg.mqttPass(),
                      willTopic.c_str(), 0, true, "offline"))
    {
        _mqtt.publish(willTopic.c_str(), "online", true);
        String sub = String(_cfg.name()) + "/set/#";
        _mqtt.subscribe(sub.c_str());
        if (!_haDiscoverySent) {
            publishHADiscovery(_mqtt, _cfg.name());
            _haDiscoverySent = true;
        }
    }
}

void MqttManager::publish() {
    if (!_mqtt.connected()) return;
    String json  = buildJSON();
    String topic = String(_cfg.name()) + "/state";
    _mqtt.publish(topic.c_str(), json.c_str());
}

String MqttManager::buildJSON() {
    const InverterData& d = _inv.data();

    JsonDocument doc;
    // System
    doc["run_state"]      = d.runState;
    doc["inverter_temp"]  = d.inverterTemp;
    doc["heatsink_temp"]  = d.heatsinkTemp;
    // Grid
    doc["grid_freq"]      = serialized(String(d.gridFreq, 2));
    doc["inverter_power"] = d.inverterPower;
    doc["grid_power"]     = d.gridPower;
    doc["grid_voltage"]   = serialized(String(d.gridVoltage, 1));
    doc["load_power"]     = d.loadPower;
    // PV
    doc["pv1_voltage"]    = serialized(String(d.pv1Voltage, 1));
    doc["pv1_current"]    = serialized(String(d.pv1Current, 2));
    doc["pv1_power"]      = d.pv1Power;
    doc["pv2_voltage"]    = serialized(String(d.pv2Voltage, 1));
    doc["pv2_current"]    = serialized(String(d.pv2Current, 2));
    doc["pv2_power"]      = d.pv2Power;
    doc["pv_total"]       = d.pvPower;
    // Battery 1
    doc["batt_voltage"]   = serialized(String(d.battVoltage, 1));
    doc["batt_current"]   = serialized(String(d.battCurrent, 2));
    doc["batt_power"]     = d.batteryPower;
    doc["batt_temp"]      = d.battTemp;
    doc["batt_soc"]       = d.batterySOC;
    doc["batt_soh"]       = d.battSOH;
    doc["batt_cycles"]    = d.battCycles;
    // Battery 2
    doc["batt2_voltage"]  = serialized(String(d.batt2Voltage, 1));
    doc["batt2_current"]  = serialized(String(d.batt2Current, 2));
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
    doc["today_gen"]      = serialized(String(d.todayGeneration, 2));
    doc["total_gen"]      = serialized(String(d.totalGeneration, 1));
    doc["today_use"]      = serialized(String(d.todayConsumption, 2));
    doc["total_use"]      = serialized(String(d.totalConsumption, 1));
    doc["today_imp"]      = serialized(String(d.todayImport, 2));
    doc["total_imp"]      = serialized(String(d.totalImport, 1));
    doc["today_exp"]      = serialized(String(d.todayExport, 2));
    doc["total_exp"]      = serialized(String(d.totalExport, 1));
    doc["today_chg"]      = serialized(String(d.todayCharged, 2));
    doc["total_chg"]      = serialized(String(d.totalCharged, 1));
    doc["today_dis"]      = serialized(String(d.todayDischarged, 2));
    doc["total_dis"]      = serialized(String(d.totalDischarged, 1));
    // Mode & control
    doc["working_mode"]        = d.workingMode;
    doc["battery_save"]        = _bs.isActive();
    doc["battery_save_target"] = _bs.targetPower();
    // Status
    doc["modbus_ok"]  = !_inv.hasError();
    doc["mqtt_ok"]    = _mqtt.connected();
    doc["wifi_ok"]    = WiFi.isConnected();
    doc["uptime"]     = millis();

    String out;
    serializeJson(doc, out);
    return out;
}
