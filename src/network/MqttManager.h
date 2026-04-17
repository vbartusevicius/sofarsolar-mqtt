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
    void loop()    { if (_ready) _mqtt.loop(); }
    void connect();
    void publish();

    bool ready()     const { return _ready; }
    bool connected() { return _mqtt.connected(); }

    // Control commands (callable from web + MQTT)
    void setMode(const String& mode);
    void setCharge(int32_t watts);
    void setAuto(int32_t limit);

    const char* currentMode()  const { return _mode; }
    int32_t chargePower()      const { return _chargePower; }
    int32_t autoLimit()        const { return _autoLimit; }

    String buildJSON();

private:
    EEConfig&     _cfg;
    Inverter&     _inv;
    BatterySaver& _bs;
    Modbus&       _mb;

    WiFiClient    _wifiClient;
    PubSubClient  _mqtt;
    bool          _ready           = false;
    bool          _haDiscoverySent = false;
    char          _mode[16]        = "auto";
    int32_t       _chargePower     = 0;
    int32_t       _autoLimit       = 16384;

    static MqttManager* _instance;
    static void callbackTrampoline(char* topic, byte* payload, unsigned int len);
    void handleMessage(const String& topic, const String& msg);
};

#endif // SOFAR_MQTT_MANAGER_H
