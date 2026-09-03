#ifndef SOFAR_MQTT_MANAGER_H
#define SOFAR_MQTT_MANAGER_H

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>

class EEConfig;
class Inverter;
class BatterySaver;
class ModeController;

class MqttManager {
public:
    MqttManager(EEConfig& cfg, Inverter& inv, BatterySaver& bs, ModeController& ctrl);

    void begin();
    void loop();
    void connect();
    void publish();

    bool ready()     const { return _ready; }
    bool connected() { return _mqtt.connected(); }

    String buildJSON();

private:
    EEConfig&       _cfg;
    Inverter&       _inv;
    BatterySaver&   _bs;
    ModeController& _ctrl;

    WiFiClient    _wifiClient;
    PubSubClient  _mqtt;
    bool          _ready = false;

    // End-to-end liveness through the broker (self-echo ping)
    unsigned long _lastPingAt = 0;
    uint32_t      _pingSeq    = 0;
    uint32_t      _echoSeq    = 0;

    void fillState(JsonDocument& doc);
    void checkLiveness();
    void sendEchoPing();
    void forceReconnect(const char* reason);

    static MqttManager* _instance;
    static void callbackTrampoline(char* topic, byte* payload, unsigned int len);
    void handleMessage(const String& topic, const String& msg);
};

#endif
