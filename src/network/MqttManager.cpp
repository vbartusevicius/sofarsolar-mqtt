#include "network/MqttManager.h"
#include <ArduinoJson.h>
#include "util/HeapStats.h"
#include "config/EEConfig.h"
#include "inverter/Inverter.h"
#include "inverter/TelemetrySchema.h"
#include "control/BatterySaver.h"
#include "control/ModeController.h"
#include "ha/Discovery.h"
#include "util/AppLog.h"
#include "util/ChunkedWriter.h"
#include "Version.h"

MqttManager* MqttManager::_instance = nullptr;

MqttManager::MqttManager(EEConfig& cfg, Inverter& inv,
                         BatterySaver& bs, ModeController& ctrl)
    : _cfg(cfg), _inv(inv), _bs(bs), _ctrl(ctrl), _mqtt(_wifiClient)
{
    _instance = this;
}

static bool endsWith(const char* str, const char* suffix) {
    size_t slen = strlen(str), xlen = strlen(suffix);
    return slen >= xlen && strcmp(str + slen - xlen, suffix) == 0;
}

void MqttManager::callbackTrampoline(char* topic, byte* payload,
                                     unsigned int len)
{
    if (!_instance || !topic) return;
    char msg[64];
    unsigned int copyLen = len < sizeof(msg) - 1 ? len : sizeof(msg) - 1;
    if (copyLen > 0 && payload) memcpy(msg, payload, copyLen);
    msg[copyLen] = '\0';
    _instance->handleMessage(topic, msg);
}

void MqttManager::handleMessage(const char* topic, const char* msg) {
    if (endsWith(topic, "/ping")) {
        uint32_t seq = (uint32_t)strtoul(msg, nullptr, 10);
        if ((uint32_t)(seq - _echoSeq) < 0x80000000UL) _echoSeq = seq;
        return;
    }
    if (endsWith(topic, "/set/battery_save")) {
        bool on = strcmp(msg, "on") == 0 || strcmp(msg, "true") == 0 ||
                  strcmp(msg, "1") == 0;
        if (on)                  _ctrl.setMode("battery_saver");
        else if (_bs.isActive()) _ctrl.setMode("auto");
    }
    else if (endsWith(topic, "/set/charge"))  { _ctrl.setCharge(atol(msg)); }
    else if (endsWith(topic, "/set/standby")) { _ctrl.setMode("standby"); }
    else if (endsWith(topic, "/set/auto"))    { _ctrl.setAuto(atol(msg)); }
    else if (endsWith(topic, "/set/mode"))    { _ctrl.setMode(msg); }
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

    // stream to the socket in chunks, no intermediate String
    size_t len = measureJson(doc);
    bool ok = _mqtt.beginPublish(topic, len, false);
    if (ok) {
        ChunkedWriter<PubSubClient, 256> out(_mqtt);
        serializeJson(doc, out);
        ok = out.finish() && _mqtt.endPublish();
    }

    char lb[48];
    snprintf(lb, sizeof(lb), "Pub %s len=%u%s", ok ? "OK" : "FAIL",
             (unsigned)len, ok ? "" : " (socket)");
    appLog.add("MQTT", lb);
    // A half-written packet desyncs the MQTT stream; drop the session
    // rather than block on the same stalled socket every 10 s.
    if (!ok) forceReconnect("publish write failed");
}

void MqttManager::fillState(JsonDocument& doc) {
    const InverterData& d = _inv.data();
    char fb[16];

#define FILL_INT_FIELD(k, m, n, u, dc, sc)      doc[k] = d.m;
#define FILL_FLT_FIELD(k, m, dec, n, u, dc, sc) doc[k] = serialized(dtostrf(d.m, 0, dec, fb));
    SOFAR_TELEMETRY_INT_FIELDS(FILL_INT_FIELD)
    SOFAR_TELEMETRY_FLOAT_FIELDS(FILL_FLT_FIELD)
#undef FILL_INT_FIELD
#undef FILL_FLT_FIELD

    // Mode & control
    doc["battery_save"]        = _bs.isActive();
    doc["battery_save_target"] = _bs.targetPower();
    doc["bsave_strategy"]      = _bs.strategyName();
    // Control state
    doc["mode"]          = _ctrl.currentMode();
    doc["charge_power"]  = _ctrl.chargePower();
    doc["auto_limit"]    = _ctrl.autoLimit();
    // Status
    doc["serial_number"] = _inv.serialNumber();
    doc["firmware"]      = FW_VERSION;
    doc["modbus_ok"]     = !_inv.hasError();
    doc["passive_mode_ok"] = _inv.isPassiveMode();
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
