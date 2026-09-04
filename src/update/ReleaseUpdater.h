#ifndef SOFAR_RELEASE_UPDATER_H
#define SOFAR_RELEASE_UPDATER_H

#include <Arduino.h>
#include "Config.h"

// OTA from the GitHub repo's latest release asset
//   https://github.com/<repo>/releases/download/<tag>/firmware.bin
//
// A TLS download needs ~22 KB contiguous heap + ~6 KB stack, which is not
// available once WiFiManager/display/MQTT/web are up.  So a runtime check
// only parks the release tag in RTC memory and reboots; the flash itself
// runs from flashPendingAtBoot() before anything else allocates.
class ReleaseUpdater {
public:
    static constexpr unsigned long FIRST_CHECK_DELAY_MS = 120000;

    void flashPendingAtBoot();

    void run();
    String checkNow();

    void     setCheckIntervalS(uint32_t s);
    uint32_t checkIntervalS() const { return _checkIntervalMs / 1000; }

private:
    unsigned long _nextCheckAt     = FIRST_CHECK_DELAY_MS;
    uint32_t      _checkIntervalMs = OTA_CHECK_INTERVAL_S * 1000UL;

    static constexpr const char* GITHUB_REPO = "vbartusevicius/sofarsolar-mqtt";

    String checkForUpdates();
    bool   fetchLatestTag(String& tagOut);
    bool   resolveAssetUrl(const String& tag, String& urlOut);
    bool   downloadAndFlash(const String& url);
    bool   connectStoredWiFi(uint32_t timeoutMs);
};

#endif // SOFAR_RELEASE_UPDATER_H
