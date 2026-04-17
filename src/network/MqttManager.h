#ifndef SOFAR_MQTT_MANAGER_H
#define SOFAR_MQTT_MANAGER_H

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>

class EEConfig;
class Inverter;
class BatterySaver;
class Modbus;

class MqttManager {
public:
    MqttManager(EEConfig& cfg, Inverter& inv, BatterySaver& bs, Modbus& mb);

    void begin();
    void loop()    { _mqtt.loop(); }
    void connect();
    void publish();

    bool connected() { return _mqtt.connected(); }

    String buildJSON();

private:
    EEConfig&     _cfg;
    Inverter&     _inv;
    BatterySaver& _bs;
    Modbus&       _mb;

    WiFiClient    _wifiClient;
    PubSubClient  _mqtt;
    bool          _haDiscoverySent = false;

    static MqttManager* _instance;
    static void callbackTrampoline(char* topic, byte* payload, unsigned int len);
    void handleMessage(const String& topic, const String& msg);
};

#endif // SOFAR_MQTT_MANAGER_H
