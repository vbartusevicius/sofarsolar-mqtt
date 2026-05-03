#ifndef SOFAR_HA_DISCOVERY_H
#define SOFAR_HA_DISCOVERY_H

#include <Arduino.h>
#include <PubSubClient.h>

void publishHADiscovery(PubSubClient& mqtt, const char* deviceName);

#endif
