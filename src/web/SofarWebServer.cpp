#include "web/SofarWebServer.h"
#include <ArduinoJson.h>
#include "Config.h"
#include "config/EEConfig.h"
#include "control/BatterySaver.h"
#include "network/MqttManager.h"
#include "web/WebPage.h"

SofarWebServer::SofarWebServer(EEConfig& cfg, BatterySaver& bs,
                               MqttManager& mqtt)
    : _server(80), _cfg(cfg), _bs(bs), _mqtt(mqtt)
{}

void SofarWebServer::begin() {
    _server.on("/", [this]() {
        _server.send_P(200, "text/html", PAGE_HTML);
    });

    _server.on("/json", [this]() {
        _server.send(200, "application/json", _mqtt.buildJSON());
    });

    _server.on("/settings", [this]() {
        JsonDocument doc;
        doc["mqtthost"]   = _cfg.mqttHost();
        doc["mqttport"]   = _cfg.mqttPort();
        doc["mqttuser"]   = _cfg.mqttUser();
        doc["mqttpass"]   = _cfg.mqttPass();
        doc["deviceName"] = _cfg.name();
        String out;
        serializeJson(doc, out);
        _server.send(200, "application/json", out);
    });

    _server.on("/command", [this]() {
        if (_server.hasArg("mqtthost"))   _server.arg("mqtthost").toCharArray(_cfg.mqttHost(), EE_HOST_LEN);
        if (_server.hasArg("mqttport"))   _server.arg("mqttport").toCharArray(_cfg.mqttPort(), EE_PORT_LEN);
        if (_server.hasArg("mqttuser"))   _server.arg("mqttuser").toCharArray(_cfg.mqttUser(), EE_USER_LEN);
        if (_server.hasArg("mqttpass"))   _server.arg("mqttpass").toCharArray(_cfg.mqttPass(), EE_PASS_LEN);
        if (_server.hasArg("deviceName")) _server.arg("deviceName").toCharArray(_cfg.name(), EE_NAME_LEN);
        _cfg.save();
        _server.send(200, "text/plain", "OK");
        delay(500);
        ESP.reset();
    });

    _server.on("/api/battery_save", [this]() {
        _bs.toggle();
        JsonDocument doc;
        doc["battery_save"] = _bs.isActive();
        String out;
        serializeJson(doc, out);
        _server.send(200, "application/json", out);
    });

    _server.begin();
}
