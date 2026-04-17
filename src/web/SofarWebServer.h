#ifndef SOFAR_WEB_SERVER_H
#define SOFAR_WEB_SERVER_H

#include <ESP8266WebServer.h>

class EEConfig;
class BatterySaver;
class MqttManager;

class SofarWebServer {
public:
    SofarWebServer(EEConfig& cfg, BatterySaver& bs, MqttManager& mqtt);

    void begin();
    void handleClient() { _server.handleClient(); }

private:
    ESP8266WebServer _server;
    EEConfig&        _cfg;
    BatterySaver&    _bs;
    MqttManager&     _mqtt;
};

#endif // SOFAR_WEB_SERVER_H
