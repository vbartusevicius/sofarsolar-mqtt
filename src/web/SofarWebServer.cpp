#include "web/SofarWebServer.h"
#include <ArduinoJson.h>
#include <Updater.h>
#include <WiFiUdp.h>
#include "Config.h"
#include "config/EEConfig.h"
#include "inverter/Inverter.h"
#include "control/BatterySaver.h"
#include "network/MqttManager.h"
#include "control/Command.h"
#include "control/ModeController.h"
#include "display/Display.h"
#include "telemetry/LinkStatusNow.h"
#include "telemetry/StateJson.h"
#include "WebPage.gz.h"   // generated into the build dir by tools/embed_web.py
#include "util/AppLog.h"
#include "util/ChunkedWriter.h"
#include "Version.h"

SofarWebServer::SofarWebServer(EEConfig& cfg, Inverter& inv,
                               BatterySaver& bs, MqttManager& mqtt,
                               ModeController& ctrl, Display& disp)
    : _server(80), _cfg(cfg), _inv(inv), _bs(bs), _mqtt(mqtt), _ctrl(ctrl),
      _disp(disp)
{}

void SofarWebServer::sendJson(const JsonDocument& doc) {
    _server.setContentLength(measureJson(doc));
    _server.send(200, "application/json", "");
    ChunkedWriter<WiFiClient, 256> out(_server.client());
    serializeJson(doc, out);
    out.finish();
}

void SofarWebServer::begin() {
    _server.on("/", [this]() {
        _server.sendHeader("Content-Encoding", "gzip");
        _server.send_P(200, "text/html", (PGM_P)PAGE_GZ, PAGE_GZ_LEN);
    });

    // Streamed straight to the socket: the web UI polls this every 5 s, and a
    // ~1.5 kB String per poll churned the heap for as long as a tab was open.
    _server.on("/json", [this]() {
        JsonDocument doc;
        fillStateJson(doc, _inv, _bs, _ctrl, linkStatusNow(_mqtt.connected()));
        sendJson(doc);
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
        doc["passiveTimeoutS"] = _inv.passiveTimeoutS();
        doc["timeoutAction"]   = _inv.timeoutAction();
        doc["keepaliveS"]      = _inv.keepaliveMs() / 1000;
        doc["idleLapseMin"]  = _bs.idleLapseMs() / 60000;
        sendJson(doc);
    });

    _server.on("/command", [this]() {
        bool any = false;
        if (_server.hasArg("mqtthost"))   { _cfg.setMqttHost(_server.arg("mqtthost").c_str()); any = true; }
        if (_server.hasArg("mqttport"))   { _cfg.setMqttPort(_server.arg("mqttport").c_str()); any = true; }
        if (_server.hasArg("mqttuser"))   { _cfg.setMqttUser(_server.arg("mqttuser").c_str()); any = true; }
        if (_server.hasArg("mqttpass"))   { _cfg.setMqttPass(_server.arg("mqttpass").c_str()); any = true; }
        if (_server.hasArg("deviceName")) { _cfg.setName(_server.arg("deviceName").c_str());   any = true; }
        if (!any) { _server.send(400, "text/plain", "no parameters"); return; }
        _cfg.save();   // commits to flash only if a value actually changed
        _server.send(200, "text/plain", "OK");
        delay(500);
        ESP.reset();
    });

    // Same keys and parser as MQTT "<device>/set/<key>": /api/<key>?v=<value>
    static const char* const CONTROL_KEYS[] = { "battery_save", "mode", "charge", "auto" };
    for (const char* key : CONTROL_KEYS) {
        char path[24];
        snprintf(path, sizeof(path), "/api/%s", key);
        _server.on(path, [this, key]() {
            Command cmd;
            String v = _server.arg("v");
            if (!parseCommand(key, v.c_str(), cmd)) {
                _server.send(400, "text/plain", "invalid value");
                return;
            }
            _ctrl.apply(cmd);
            JsonDocument doc;
            doc["mode"]         = _ctrl.modeName();
            doc["battery_save"] = _ctrl.saverActive();
            doc["charge_power"] = _ctrl.chargePower();
            doc["auto_limit"]   = _ctrl.autoLimit();
            sendJson(doc);
        });
    }

    // applies immediately, no reboot; save() commits only on real changes
    _server.on("/api/tuning", [this]() {
        bool any = false;
        if (_server.hasArg("delta"))     { _bs.setMinDelta(_server.arg("delta").toInt());                _cfg.setBsaveDelta(_bs.minDelta());       any = true; }
        if (_server.hasArg("maxpower"))  { _bs.setMaxPower(_server.arg("maxpower").toInt());             _cfg.setBsaveMaxPower(_bs.maxPower());    any = true; }
        if (_server.hasArg("lapse"))     { _bs.setIdleLapse(_server.arg("lapse").toInt() * 60000UL);     _cfg.setIdleLapseMs(_bs.idleLapseMs());   any = true; }
        if (any) _cfg.save();
        JsonDocument doc;
        doc["bsDelta"]      = _bs.minDelta();
        doc["bsMaxPower"]   = _bs.maxPower();
        doc["keepaliveS"]   = _inv.keepaliveMs() / 1000;
        doc["idleLapseMin"] = _bs.idleLapseMs() / 60000;
        sendJson(doc);
    });

    // Starts the on-screen wizard; the result is persisted by the callback
    // Display was given in setup(), so nothing to do here but kick it off.
    _server.on("/api/touch_cal", HTTP_POST, [this]() {
        _disp.startCalibration();
        _server.send(200, "application/json",
                     "{\"status\":\"ok\",\"message\":\"tap the 3 crosshairs on the LCD\"}");
    });

    // Chunked from the ring buffer: the web UI polls this every 5 s, and a
    // 2 kB String per poll fragmented the heap over long uptimes.
    _server.on("/log", [this]() {
        _server.setContentLength(CONTENT_LENGTH_UNKNOWN);
        _server.send(200, "text/plain", "");
        char chunk[129];
        unsigned int col = 0, e = appLog.end();
        for (unsigned int i = 0; i < e; i++) {
            chunk[col++] = appLog.at(i);
            if (col < sizeof(chunk) - 1) continue;
            chunk[col] = '\0';
            _server.sendContent(chunk);
            col = 0;
        }
        if (col > 0) { chunk[col] = '\0'; _server.sendContent(chunk); }
        _server.sendContent("");
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
