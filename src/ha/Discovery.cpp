#include "Discovery.h"
#include <ESP8266WiFi.h>
#include <ArduinoJson.h>
#include "inverter/TelemetrySchema.h"
#include "util/ChunkedWriter.h"
#include "Version.h"

struct HaSensor {
    const char* id;
    const char* name;
    const char* unit;
    const char* devClass;   // HA device_class  (nullptr = omit)
    const char* stateClass; // "measurement" | "total_increasing" | nullptr
};

#define HA_INT_ROW(k, m, n, u, dc, sc)      { k, n, u, dc, sc },
#define HA_FLT_ROW(k, m, dec, n, u, dc, sc) { k, n, u, dc, sc },

static const HaSensor SENSORS[] = {
    SOFAR_TELEMETRY_INT_FIELDS(HA_INT_ROW)
    SOFAR_TELEMETRY_FLOAT_FIELDS(HA_FLT_ROW)
    { "battery_save_target", "Battery Save Target", "W", "power", "measurement" },
    { "firmware",            "Firmware Version",    "",  nullptr, nullptr },
};

#undef HA_INT_ROW
#undef HA_FLT_ROW

static const uint8_t SENSOR_COUNT = sizeof(SENSORS) / sizeof(SENSORS[0]);

static void publishRetainedJson(PubSubClient& mqtt, const char* topic,
                                const JsonDocument& doc)
{
    size_t len = measureJson(doc);
    if (mqtt.beginPublish(topic, len, true)) {
        ChunkedWriter<PubSubClient, 256> out(mqtt);
        serializeJson(doc, out);
        // beginPublish() fails fast once disconnected, so the remaining
        // ~50 configs cost nothing instead of one socket timeout each.
        if (!out.finish()) mqtt.disconnect();
        else mqtt.endPublish();
    }
    yield();
}

static void addDevice(JsonDocument& doc, const char* deviceName) {
    char nodeId[24];
    snprintf(nodeId, sizeof(nodeId), "sofar_%x", (unsigned)ESP.getChipId());
    IPAddress ip = WiFi.localIP();
    char cu[32];
    snprintf(cu, sizeof(cu), "http://%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);

    JsonObject dev = doc["dev"].to<JsonObject>();
    JsonArray ids = dev["ids"].to<JsonArray>();
    ids.add(nodeId);
    dev["name"] = deviceName;
    dev["mf"]   = "Sofar Solar";
    dev["mdl"]  = "HYD 20 KTL";
    dev["sw"]   = FW_VERSION;
    dev["cu"]   = cu;
}

static void publishSensor(PubSubClient& mqtt, const char* deviceName,
                          const HaSensor& s)
{
    unsigned chipId = (unsigned)ESP.getChipId();
    char uid[48], topic[80], statT[80], valTpl[48];
    snprintf(uid,    sizeof(uid),    "sofar_%x_%s", chipId, s.id);
    snprintf(topic,  sizeof(topic),  "homeassistant/sensor/sofar_%x/%s/config", chipId, s.id);
    snprintf(statT,  sizeof(statT),  "%s/state", deviceName);
    snprintf(valTpl, sizeof(valTpl), "{{ value_json.%s }}", s.id);

    JsonDocument doc;
    doc["name"]    = s.name;
    doc["uniq_id"] = uid;
    doc["stat_t"]  = statT;
    doc["val_tpl"] = valTpl;
    if (s.unit && s.unit[0])
        doc["unit_of_meas"] = s.unit;
    if (s.devClass)
        doc["dev_cla"]  = s.devClass;
    if (s.stateClass)
        doc["stat_cla"] = s.stateClass;
    addDevice(doc, deviceName);

    publishRetainedJson(mqtt, topic, doc);
}

static void publishSwitch(PubSubClient& mqtt, const char* deviceName)
{
    unsigned chipId = (unsigned)ESP.getChipId();
    char uid[48], topic[80], statT[80], cmdT[80];
    snprintf(uid,   sizeof(uid),   "sofar_%x_battery_save", chipId);
    snprintf(topic, sizeof(topic), "homeassistant/switch/sofar_%x/battery_save/config", chipId);
    snprintf(statT, sizeof(statT), "%s/state", deviceName);
    snprintf(cmdT,  sizeof(cmdT),  "%s/set/battery_save", deviceName);

    JsonDocument doc;
    doc["name"]    = "Battery Saver";
    doc["uniq_id"] = uid;
    doc["stat_t"]  = statT;
    doc["val_tpl"] = "{{ 'ON' if value_json.battery_save else 'OFF' }}";
    doc["cmd_t"]   = cmdT;
    doc["pl_on"]   = "on";
    doc["pl_off"]  = "off";
    doc["icon"]    = "mdi:battery-sync";
    addDevice(doc, deviceName);

    publishRetainedJson(mqtt, topic, doc);
}

static void publishModeSelect(PubSubClient& mqtt, const char* deviceName)
{
    unsigned chipId = (unsigned)ESP.getChipId();
    char uid[48], topic[80], statT[80], cmdT[80];
    snprintf(uid,   sizeof(uid),   "sofar_%x_mode", chipId);
    snprintf(topic, sizeof(topic), "homeassistant/select/sofar_%x/mode/config", chipId);
    snprintf(statT, sizeof(statT), "%s/state", deviceName);
    snprintf(cmdT,  sizeof(cmdT),  "%s/set/mode", deviceName);

    JsonDocument doc;
    doc["name"]    = "Inverter Mode";
    doc["uniq_id"] = uid;
    doc["stat_t"]  = statT;
    doc["val_tpl"] = "{{ value_json.mode }}";
    doc["cmd_t"]   = cmdT;
    JsonArray opts = doc["ops"].to<JsonArray>();
    opts.add("auto");
    opts.add("charge");
    opts.add("standby");
    opts.add("battery_saver");
    doc["icon"]    = "mdi:solar-power";
    addDevice(doc, deviceName);

    publishRetainedJson(mqtt, topic, doc);
}

static void publishNumber(PubSubClient& mqtt, const char* deviceName,
                          const char* id, const char* name,
                          int32_t minVal, int32_t maxVal,
                          const char* unit, const char* icon,
                          const char* cmdSuffix)
{
    unsigned chipId = (unsigned)ESP.getChipId();
    char uid[48], topic[80], statT[80], valTpl[48], cmdT[80];
    snprintf(uid,    sizeof(uid),    "sofar_%x_%s", chipId, id);
    snprintf(topic,  sizeof(topic),  "homeassistant/number/sofar_%x/%s/config", chipId, id);
    snprintf(statT,  sizeof(statT),  "%s/state", deviceName);
    snprintf(valTpl, sizeof(valTpl), "{{ value_json.%s }}", id);
    snprintf(cmdT,   sizeof(cmdT),   "%s%s", deviceName, cmdSuffix);

    JsonDocument doc;
    doc["name"]    = name;
    doc["uniq_id"] = uid;
    doc["stat_t"]  = statT;
    doc["val_tpl"] = valTpl;
    doc["cmd_t"]   = cmdT;
    doc["min"]     = minVal;
    doc["max"]     = maxVal;
    doc["step"]    = 100;
    doc["unit_of_meas"] = unit;
    doc["icon"]    = icon;
    addDevice(doc, deviceName);

    publishRetainedJson(mqtt, topic, doc);
}

void publishHADiscovery(PubSubClient& mqtt, const char* deviceName) {
    if (!mqtt.connected()) return;

    for (uint8_t i = 0; i < SENSOR_COUNT; i++) {
        publishSensor(mqtt, deviceName, SENSORS[i]);
    }
    publishSwitch(mqtt, deviceName);
    publishModeSelect(mqtt, deviceName);
    publishNumber(mqtt, deviceName,
                  "charge_power", "Charge Power",
                  -20000, 20000, "W", "mdi:battery-charging", "/set/charge");
    publishNumber(mqtt, deviceName,
                  "auto_limit", "Auto Power Limit",
                  100, 20000, "W", "mdi:gauge", "/set/auto");
}
