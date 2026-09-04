#ifndef SOFAR_WEB_SERVER_H
#define SOFAR_WEB_SERVER_H

#include <ESP8266WebServer.h>

class EEConfig;
class Inverter;
class BatterySaver;
class MqttManager;
class ModeController;
class ReleaseUpdater;

class SofarWebServer {
public:
    SofarWebServer(EEConfig& cfg, Inverter& inv, BatterySaver& bs,
                   MqttManager& mqtt, ModeController& ctrl,
                   ReleaseUpdater& updater);

    void begin();
    void handleClient() { if (!_paused) _server.handleClient(); }
    void pause();
    void resume();

private:
    ESP8266WebServer _server;
    bool             _paused = false;
    EEConfig&        _cfg;
    Inverter&        _inv;
    BatterySaver&    _bs;
    MqttManager&     _mqtt;
    ModeController&  _ctrl;
    ReleaseUpdater&  _updater;
};

#endif
