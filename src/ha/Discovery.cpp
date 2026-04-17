#include "Discovery.h"
#include <ESP8266WiFi.h>
#include <ArduinoJson.h>

// ── Sensor descriptor (stored in flash) ──────────────────────
struct HaSensor {
    const char* id;
    const char* name;
    const char* unit;
    const char* devClass;   // HA device_class  (nullptr = omit)
    const char* stateClass; // "measurement" | "total_increasing" | nullptr
};

// Macro to keep table readable
#define S(i, n, u, dc, sc) { i, n, u, dc, sc }

static const HaSensor SENSORS[] PROGMEM = {
    // System
    S("run_state",       "Run State",            "",    nullptr,       "measurement"),
    S("inverter_temp",   "Inverter Temperature", "°C",  "temperature", "measurement"),
    S("heatsink_temp",   "Heatsink Temperature", "°C",  "temperature", "measurement"),
    // Grid
    S("grid_freq",       "Grid Frequency",       "Hz",  "frequency",   "measurement"),
    S("inverter_power",  "Inverter Power",       "W",   "power",       "measurement"),
    S("grid_power",      "Grid Power",           "W",   "power",       "measurement"),
    S("grid_voltage",    "Grid Voltage",         "V",   "voltage",     "measurement"),
    S("load_power",      "Load Power",           "W",   "power",       "measurement"),
    // PV
    S("pv1_voltage",     "PV1 Voltage",          "V",   "voltage",     "measurement"),
    S("pv1_current",     "PV1 Current",          "A",   "current",     "measurement"),
    S("pv1_power",       "PV1 Power",            "W",   "power",       "measurement"),
    S("pv2_voltage",     "PV2 Voltage",          "V",   "voltage",     "measurement"),
    S("pv2_current",     "PV2 Current",          "A",   "current",     "measurement"),
    S("pv2_power",       "PV2 Power",            "W",   "power",       "measurement"),
    S("pv_total",        "Solar Power Total",    "W",   "power",       "measurement"),
    // Battery 1
    S("batt_voltage",    "Battery Voltage",      "V",   "voltage",     "measurement"),
    S("batt_current",    "Battery Current",      "A",   "current",     "measurement"),
    S("batt_power",      "Battery Power",        "W",   "power",       "measurement"),
    S("batt_temp",       "Battery Temperature",  "°C",  "temperature", "measurement"),
    S("batt_soc",        "Battery SOC",          "%",   "battery",     "measurement"),
    S("batt_soh",        "Battery SOH",          "%",   nullptr,       "measurement"),
    S("batt_cycles",     "Battery Cycles",       "",    nullptr,       "measurement"),
    // Battery 2
    S("batt2_voltage",   "Battery 2 Voltage",    "V",   "voltage",     "measurement"),
    S("batt2_current",   "Battery 2 Current",    "A",   "current",     "measurement"),
    S("batt2_power",     "Battery 2 Power",      "W",   "power",       "measurement"),
    S("batt2_temp",      "Battery 2 Temperature","°C",  "temperature", "measurement"),
    S("batt2_soc",       "Battery 2 SOC",        "%",   "battery",     "measurement"),
    S("batt2_soh",       "Battery 2 SOH",        "%",   nullptr,       "measurement"),
    S("batt2_cycles",    "Battery 2 Cycles",     "",    nullptr,       "measurement"),
    // Battery totals
    S("batt_total_power","Battery Total Power",  "W",   "power",       "measurement"),
    S("batt_avg_soc",    "Battery Avg SOC",      "%",   "battery",     "measurement"),
    S("batt_avg_soh",    "Battery Avg SOH",      "%",   nullptr,       "measurement"),
    // Energy counters
    S("today_gen",       "PV Generation Today",    "kWh", "energy",      "total_increasing"),
    S("total_gen",       "PV Generation Total",    "kWh", "energy",      "total_increasing"),
    S("today_use",       "Consumption Today",      "kWh", "energy",      "total_increasing"),
    S("total_use",       "Consumption Total",      "kWh", "energy",      "total_increasing"),
    S("today_imp",       "Import Today",           "kWh", "energy",      "total_increasing"),
    S("total_imp",       "Import Total",           "kWh", "energy",      "total_increasing"),
    S("today_exp",       "Export Today",           "kWh", "energy",      "total_increasing"),
    S("total_exp",       "Export Total",           "kWh", "energy",      "total_increasing"),
    S("today_chg",       "Battery Charged Today",  "kWh", "energy",      "total_increasing"),
    S("total_chg",       "Battery Charged Total",  "kWh", "energy",      "total_increasing"),
    S("today_dis",       "Battery Discharged Today","kWh","energy",      "total_increasing"),
    S("total_dis",       "Battery Discharged Total","kWh","energy",      "total_increasing"),
    // Mode
    S("working_mode",    "Working Mode",            "",   nullptr,       "measurement"),
    S("battery_save_target","Battery Save Target",  "W",  "power",       "measurement"),
};

static const uint8_t SENSOR_COUNT = sizeof(SENSORS) / sizeof(SENSORS[0]);

// ── Add shared device block to doc ──────────────────────────
static void addDevice(JsonDocument& doc, const char* deviceName) {
    String chipId = String(ESP.getChipId(), HEX);
    JsonObject dev = doc["dev"].to<JsonObject>();
    JsonArray ids = dev["ids"].to<JsonArray>();
    ids.add("sofar_" + chipId);
    dev["name"] = deviceName;
    dev["mf"]   = "Sofar Solar";
    dev["mdl"]  = "HYD 20 KTL";
    dev["sw"]   = "2.0.0";
    dev["cu"]   = "http://" + WiFi.localIP().toString();
}

// ── Publish one sensor config ────────────────────────────────
static void publishSensor(PubSubClient& mqtt, const char* deviceName,
                          const HaSensor& s)
{
    String chipId = String(ESP.getChipId(), HEX);
    String uid    = "sofar_" + chipId + "_" + s.id;
    String topic  = "homeassistant/sensor/sofar_" + chipId + "/" + s.id + "/config";

    JsonDocument doc;
    doc["name"]    = s.name;
    doc["uniq_id"] = uid;
    doc["stat_t"]  = String(deviceName) + "/state";
    doc["val_tpl"] = "{{ value_json." + String(s.id) + " }}";
    if (s.unit && s.unit[0])
        doc["unit_of_meas"] = s.unit;
    if (s.devClass)
        doc["dev_cla"]  = s.devClass;
    if (s.stateClass)
        doc["stat_cla"] = s.stateClass;
    addDevice(doc, deviceName);

    String payload;
    serializeJson(doc, payload);
    mqtt.publish(topic.c_str(), payload.c_str(), true);
    yield();
}

// ── Publish battery-saver switch ─────────────────────────────
static void publishSwitch(PubSubClient& mqtt, const char* deviceName)
{
    String chipId = String(ESP.getChipId(), HEX);
    String uid    = "sofar_" + chipId + "_battery_save";
    String topic  = "homeassistant/switch/sofar_" + chipId + "/battery_save/config";

    JsonDocument doc;
    doc["name"]    = "Battery Saver";
    doc["uniq_id"] = uid;
    doc["stat_t"]  = String(deviceName) + "/state";
    doc["val_tpl"] = "{{ 'ON' if value_json.battery_save else 'OFF' }}";
    doc["cmd_t"]   = String(deviceName) + "/set/battery_save";
    doc["pl_on"]   = "on";
    doc["pl_off"]  = "off";
    doc["icon"]    = "mdi:battery-sync";
    addDevice(doc, deviceName);

    String payload;
    serializeJson(doc, payload);
    mqtt.publish(topic.c_str(), payload.c_str(), true);
    yield();
}

// ── Public entry point ───────────────────────────────────────
void publishHADiscovery(PubSubClient& mqtt, const char* deviceName) {
    if (!mqtt.connected()) return;

    for (uint8_t i = 0; i < SENSOR_COUNT; i++) {
        publishSensor(mqtt, deviceName, SENSORS[i]);
    }
    publishSwitch(mqtt, deviceName);
}
