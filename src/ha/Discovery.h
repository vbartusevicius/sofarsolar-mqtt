#ifndef SOFAR_HA_DISCOVERY_H
#define SOFAR_HA_DISCOVERY_H

#include <Arduino.h>
#include <PubSubClient.h>

// Publish HA MQTT auto-discovery configs for all inverter sensors,
// a battery-saver switch, and full device information.
// Call once after MQTT connects (or reconnects).
void publishHADiscovery(PubSubClient& mqtt, const char* deviceName);

#endif // SOFAR_HA_DISCOVERY_H
