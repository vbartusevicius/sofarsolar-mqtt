#include "web/SofarWebServer.h"
#include <ArduinoJson.h>
#include <Updater.h>
#include <WiFiUdp.h>
#include "Config.h"
#include "config/EEConfig.h"
#include "inverter/Inverter.h"
#include "control/BatterySaver.h"
#include "network/MqttManager.h"
#include "control/ModeController.h"
#include "display/Display.h"
#include "web/WebPage.h"
#include "util/AppLog.h"
#include "Version.h"

SofarWebServer::SofarWebServer(EEConfig& cfg, Inverter& inv,
                               BatterySaver& bs, MqttManager& mqtt,
                               ModeController& ctrl, Display& disp)
    : _server(80), _cfg(cfg), _inv(inv), _bs(bs), _mqtt(mqtt), _ctrl(ctrl),
      _disp(disp)
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
        doc["touch_cal"]     = _disp.touchCal().valid;
        doc["bsDelta"]       = _bs.minDelta();
        doc["bsMaxPower"]    = _bs.maxPower();
        doc["keepaliveS"]    = _inv.keepaliveMs() / 1000;
        doc["idleLapseMin"]  = _bs.idleLapseMs() / 60000;
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

    // Starts the on-screen wizard; the result is persisted by the callback
    // Display was given in setup(), so nothing to do here but kick it off.
    _server.on("/api/touch_cal", HTTP_POST, [this]() {
        _disp.startCalibration();
        _server.send(200, "application/json",
                     "{\"status\":\"ok\",\"message\":\"tap the 3 crosshairs on the LCD\"}");
    });

    _server.on("/log", [this]() {
        _server.send(200, "text/plain", appLog.text());
    });

    // Firmware upload: POST a .bin built by CI (or `pio run`) here.
    _server.on("/api/upload", HTTP_POST,
        [this]() {
            bool ok = !Update.hasError();
            _server.sendHeader("Connection", "close");
            _server.send(ok ? 200 : 500, "application/json",
                         ok ? "{\"status\":\"ok\"}"
                            : "{\"status\":\"error\"}");
            if (ok) {
                appLog.add("FW", "upload applied, restarting");
                delay(500);
                ESP.restart();
            }
        },
        [this]() { handleFirmwareUpload(); });

    _server.begin();
}

void SofarWebServer::handleFirmwareUpload() {
    HTTPUpload& up = _server.upload();

    if (up.status == UPLOAD_FILE_START) {
        char lb[96];
        snprintf(lb, sizeof(lb), "upload %s start free=%u blk=%u",
                 up.filename.c_str(), (unsigned)ESP.getFreeHeap(),
                 (unsigned)ESP.getMaxFreeBlockSize());
        appLog.add("FW", lb);

        // Free sockets/buffers so Update.begin() gets its contiguous block
        WiFiUDP::stopAll();
        WiFiClient::stopAllExcept(&_server.client());

        uint32_t maxSize = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
        Update.runAsync(true);
        if (!Update.begin(maxSize, U_FLASH)) {
            snprintf(lb, sizeof(lb), "Update.begin failed: %s", Update.getErrorString().c_str());
            appLog.add("FW", lb);
        }
    }
    else if (up.status == UPLOAD_FILE_WRITE) {
        if (Update.hasError()) return;
        if (Update.write(up.buf, up.currentSize) != up.currentSize) {
            char lb[80];
            snprintf(lb, sizeof(lb), "write failed: %s", Update.getErrorString().c_str());
            appLog.add("FW", lb);
        }
    }
    else if (up.status == UPLOAD_FILE_END) {
        if (Update.hasError()) return;
        char lb[80];
        if (Update.end(true)) snprintf(lb, sizeof(lb), "flashed %u bytes", (unsigned)up.totalSize);
        else                  snprintf(lb, sizeof(lb), "end failed: %s", Update.getErrorString().c_str());
        appLog.add("FW", lb);
    }
    else if (up.status == UPLOAD_FILE_ABORTED) {
        Update.end();
        appLog.add("FW", "upload aborted");
    }
    yield();
}
