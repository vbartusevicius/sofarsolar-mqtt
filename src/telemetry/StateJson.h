#ifndef SOFAR_STATE_JSON_H
#define SOFAR_STATE_JSON_H

#include <stdint.h>
#include <ArduinoJson.h>

class Inverter;
class BatterySaver;
class ModeController;

// Device-side facts the state document reports but no domain object owns
struct LinkStatus {
    bool     wifiOk   = false;
    bool     mqttOk   = false;
    uint32_t uptimeMs = 0;
    uint32_t freeHeap = 0;
    uint32_t maxBlock = 0;
    uint8_t  heapFrag = 0;
};

// The one state document: published to MQTT and served as /json, so Home
// Assistant and the web UI can never see different field sets.
void fillStateJson(JsonDocument& doc, const Inverter& inv,
                   const BatterySaver& bs, const ModeController& ctrl,
                   const LinkStatus& link);

#endif // SOFAR_STATE_JSON_H
