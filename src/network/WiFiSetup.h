#ifndef SOFAR_WIFI_SETUP_H
#define SOFAR_WIFI_SETUP_H

class EEConfig;

// Initialise WiFiManager (non-blocking) and start captive portal if needed.
void setupWiFi(EEConfig& cfg);

// Must be called every loop iteration to process the captive portal.
void wifiLoop();

#endif // SOFAR_WIFI_SETUP_H
