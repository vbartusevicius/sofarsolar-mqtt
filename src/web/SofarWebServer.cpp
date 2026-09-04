#include "web/SofarWebServer.h"
#include <ArduinoJson.h>
#include "Config.h"
#include "config/EEConfig.h"
#include "inverter/Inverter.h"
#include "control/BatterySaver.h"
#include "network/MqttManager.h"
#include "control/ModeController.h"
#include "update/ReleaseUpdater.h"
#include "web/WebPage.h"
#include "util/AppLog.h"
#include "Version.h"

SofarWebServer::SofarWebServer(EEConfig& cfg, Inverter& inv,
                               BatterySaver& bs, MqttManager& mqtt,
                               ModeController& ctrl, ReleaseUpdater& updater)
    : _server(80), _cfg(cfg), _inv(inv), _bs(bs), _mqtt(mqtt), _ctrl(ctrl),
      _updater(updater)
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
        doc["deviceName"]    = _cfg.name();
        doc["serialNumber"]  = _inv.serialNumber();
        doc["firmware"]      = FW_VERSION;
        doc["bsDelta"]       = _bs.minDelta();
        doc["bsMaxPower"]    = _bs.maxPower();
        doc["keepaliveS"]    = _inv.keepaliveMs() / 1000;
        doc["idleLapseMin"]  = _bs.idleLapseMs() / 60000;
        doc["otaCheckMin"]   = _updater.checkIntervalS() / 60;
        String out;
        serializeJson(doc, out);
        _server.send(200, "application/json", out);
    });

    _server.on("/command", [this]() {
        bool any = false;
        if (_server.hasArg("mqtthost"))   { _server.arg("mqtthost").toCharArray(_cfg.mqttHost(), EE_HOST_LEN); any = true; }
        if (_server.hasArg("mqttport"))   { _server.arg("mqttport").toCharArray(_cfg.mqttPort(), EE_PORT_LEN); any = true; }
        if (_server.hasArg("mqttuser"))   { _server.arg("mqttuser").toCharArray(_cfg.mqttUser(), EE_USER_LEN); any = true; }
        if (_server.hasArg("mqttpass"))   { _server.arg("mqttpass").toCharArray(_cfg.mqttPass(), EE_PASS_LEN); any = true; }
        if (_server.hasArg("deviceName")) { _server.arg("deviceName").toCharArray(_cfg.name(), EE_NAME_LEN);   any = true; }
        if (!any) { _server.send(400, "text/plain", "no parameters"); return; }
        _cfg.save();   // commits to flash only if a value actually changed
        _server.send(200, "text/plain", "OK");
        delay(500);
        ESP.reset();
    });

    _server.on("/api/battery_save", [this]() {
        _ctrl.toggleBatterySaver();
        JsonDocument doc;
        doc["battery_save"] = _bs.isActive();
        String out;
        serializeJson(doc, out);
        _server.send(200, "application/json", out);
    });

    _server.on("/api/mode", [this]() {
        if (_server.hasArg("v")) _ctrl.setMode(_server.arg("v").c_str());
        JsonDocument doc;
        doc["mode"] = _ctrl.currentMode();
        String out;
        serializeJson(doc, out);
        _server.send(200, "application/json", out);
    });

    _server.on("/api/charge", [this]() {
        if (_server.hasArg("v")) _ctrl.setCharge(_server.arg("v").toInt());
        JsonDocument doc;
        doc["charge_power"] = _ctrl.chargePower();
        doc["mode"] = _ctrl.currentMode();
        String out;
        serializeJson(doc, out);
        _server.send(200, "application/json", out);
    });

    _server.on("/api/auto", [this]() {
        if (_server.hasArg("v")) _ctrl.setAuto(_server.arg("v").toInt());
        JsonDocument doc;
        doc["auto_limit"] = _ctrl.autoLimit();
        doc["mode"] = _ctrl.currentMode();
        String out;
        serializeJson(doc, out);
        _server.send(200, "application/json", out);
    });

    // applies immediately, no reboot; save() commits only on real changes
    _server.on("/api/tuning", [this]() {
        bool any = false;
        if (_server.hasArg("delta"))     { _bs.setMinDelta(_server.arg("delta").toInt());                _cfg.setBsaveDelta(_bs.minDelta());       any = true; }
        if (_server.hasArg("maxpower"))  { _bs.setMaxPower(_server.arg("maxpower").toInt());             _cfg.setBsaveMaxPower(_bs.maxPower());    any = true; }
        if (_server.hasArg("keepalive")) { _inv.setKeepaliveMs(_server.arg("keepalive").toInt() * 1000UL); _cfg.setKeepaliveMs(_inv.keepaliveMs()); any = true; }
        if (_server.hasArg("lapse"))     { _bs.setIdleLapse(_server.arg("lapse").toInt() * 60000UL);     _cfg.setIdleLapseMs(_bs.idleLapseMs());   any = true; }
        if (any) _cfg.save();
        JsonDocument doc;
        doc["bsDelta"]      = _bs.minDelta();
        doc["bsMaxPower"]   = _bs.maxPower();
        doc["keepaliveS"]   = _inv.keepaliveMs() / 1000;
        doc["idleLapseMin"] = _bs.idleLapseMs() / 60000;
        String out;
        serializeJson(doc, out);
        _server.send(200, "application/json", out);
    });

    _server.on("/log", [this]() {
        _server.send(200, "text/plain", appLog.text());
    });

    // Interval setting is answered normally; a check that finds a release
    // reboots the device to install it, so that response never arrives.
    _server.on("/api/update", [this]() {
        if (_server.hasArg("interval")) {   // minutes
            _updater.setCheckIntervalS(_server.arg("interval").toInt() * 60UL);
            _cfg.setOtaCheckS(_updater.checkIntervalS());
            _cfg.save();
            JsonDocument doc;
            doc["current"]      = FW_VERSION;
            doc["interval_min"] = _updater.checkIntervalS() / 60;
            String out;
            serializeJson(doc, out);
            _server.send(200, "application/json", out);
            return;
        }
        JsonDocument doc;
        doc["current"]      = FW_VERSION;
        doc["interval_min"] = _updater.checkIntervalS() / 60;
        doc["updating"]     = false;
        String out;
        serializeJson(doc, out);
        _server.send(200, "application/json", out);
        _server.client().flush();
        _updater.checkNow();   // reboots if a new release is found
    });

    _server.begin();
}
