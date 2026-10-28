#include "network/MqttManager.h"
#include <ArduinoJson.h>
#include "config/EEConfig.h"
#include "inverter/Inverter.h"
#include "control/BatterySaver.h"
#include "control/Command.h"
#include "control/ModeController.h"
#include "ha/Discovery.h"
#include "telemetry/LinkStatusNow.h"
#include "telemetry/StateJson.h"
#include "util/AppLog.h"
#include "util/ChunkedWriter.h"

MqttManager::MqttManager(EEConfig& cfg, Inverter& inv,
                         BatterySaver& bs, ModeController& ctrl)
    : _cfg(cfg), _inv(inv), _bs(bs), _ctrl(ctrl), _mqtt(_wifiClient)
{}

static bool endsWith(const char* str, const char* suffix) {
    size_t slen = strlen(str), xlen = strlen(suffix);
    return slen >= xlen && strcmp(str + slen - xlen, suffix) == 0;
}

void MqttManager::onMessage(const char* topic, const uint8_t* payload,
                            unsigned int len)
{
    if (!topic) return;
    char msg[64];
    unsigned int copyLen = len < sizeof(msg) - 1 ? len : sizeof(msg) - 1;
    if (copyLen > 0 && payload) memcpy(msg, payload, copyLen);
    msg[copyLen] = '\0';

    if (endsWith(topic, "/ping")) {
        uint32_t seq = (uint32_t)strtoul(msg, nullptr, 10);
        if ((uint32_t)(seq - _echoSeq) < 0x80000000UL) _echoSeq = seq;
        return;
    }
    const char* key = mqttCommandKey(topic);
    Command cmd;
    if (key && parseCommand(key, msg, cmd)) {
        _ctrl.apply(cmd);
        return;
    }
    char lb[80];
    snprintf(lb, sizeof(lb), "ignored %s='%s'", key ? key : topic, msg);
    appLog.add("MQTT", lb);
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
    _mqtt.setCallback([this](char* t, uint8_t* p, unsigned int l) { onMessage(t, p, l); });

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
    fillStateJson(doc, _inv, _bs, _ctrl, linkStatusNow(true));
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
