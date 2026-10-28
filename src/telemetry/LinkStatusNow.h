#ifndef SOFAR_LINK_STATUS_NOW_H
#define SOFAR_LINK_STATUS_NOW_H

#include <ESP8266WiFi.h>
#include "telemetry/StateJson.h"
#include "util/HeapStats.h"

// Firmware-only: samples the live device facts for fillStateJson()
inline LinkStatus linkStatusNow(bool mqttOk) {
    LinkStatus s;
    s.wifiOk   = WiFi.isConnected();
    s.mqttOk   = mqttOk;
    s.uptimeMs = millis();
    s.freeHeap = heapStats.freeHeap;
    s.maxBlock = heapStats.maxBlock;
    s.heapFrag = heapStats.frag;
    return s;
}

#endif // SOFAR_LINK_STATUS_NOW_H
