#ifndef SOFAR_WEB_SERVER_H
#define SOFAR_WEB_SERVER_H

#include <ESP8266WebServer.h>

class EEConfig;
class Inverter;
class BatterySaver;
class MqttManager;

class SofarWebServer {
public:
    SofarWebServer(EEConfig& cfg, Inverter& inv, BatterySaver& bs, MqttManager& mqtt);

    void begin();
    void handleClient() { _server.handleClient(); }

private:
    ESP8266WebServer _server;
    EEConfig&        _cfg;
    Inverter&        _inv;
    BatterySaver&    _bs;
    MqttManager&     _mqtt;
};

#endif // SOFAR_WEB_SERVER_H
