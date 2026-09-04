#ifndef SOFAR_RELEASE_UPDATER_H
#define SOFAR_RELEASE_UPDATER_H

#include <Arduino.h>
#include "Config.h"

// Periodically checks the GitHub repository's latest release and OTA-updates
// the firmware from the release asset:
//   https://github.com/<repo>/releases/download/<tag>/firmware.bin
class ReleaseUpdater {
public:
    static constexpr unsigned long FIRST_CHECK_DELAY_MS = 120000;

    void run();
    String checkNow();
    bool     busy() const { return _busy; }

    void     setCheckIntervalS(uint32_t s);
    uint32_t checkIntervalS() const { return _checkIntervalMs / 1000; }

private:
    unsigned long _nextCheckAt     = FIRST_CHECK_DELAY_MS;
    uint32_t      _checkIntervalMs = OTA_CHECK_INTERVAL_S * 1000UL;
    bool          _busy            = false;

    // Repo that publishes release binaries ("owner/name")
    static constexpr const char* GITHUB_REPO = "vbartusevicius/sofarsolar-mqtt";

    String checkForUpdates();
    bool   fetchLatestTag(String& tagOut);
    bool   flashFromRelease(const String& tag);
};

#endif // SOFAR_RELEASE_UPDATER_H
