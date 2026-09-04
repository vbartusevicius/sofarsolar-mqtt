#include "ReleaseUpdater.h"

#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>
#include <ArduinoJson.h>

#include "util/AppLog.h"
#include "util/HeapStats.h"
#include "Version.h"

void ReleaseUpdater::setCheckIntervalS(uint32_t s) {
    if (s < 60)     s = 60;        // no faster than once a minute
    if (s > 86400)  s = 86400;     // no slower than once a day
    _checkIntervalMs = s * 1000UL;
}

void ReleaseUpdater::run() {
    long untilDue = (long)(_nextCheckAt - millis());
    if (untilDue > 0) return;
    _nextCheckAt = millis() + _checkIntervalMs;

    if (WiFi.status() != WL_CONNECTED) return;
    checkForUpdates();
}

String ReleaseUpdater::checkNow() {
    _nextCheckAt = millis() + _checkIntervalMs;   // don't double-run via timer
    if (WiFi.status() != WL_CONNECTED) return "no WiFi";
    return checkForUpdates();
}

String ReleaseUpdater::checkForUpdates() {
    String tag;
    if (!fetchLatestTag(tag)) return "check failed";

    if (tag == FW_VERSION) {
        char lb[64];
        snprintf(lb, sizeof(lb), "OTA: up to date (%s)", FW_VERSION);
        appLog.add("OTA", lb);
        return "";
    }

    char lb[80];
    snprintf(lb, sizeof(lb), "OTA: new release %s (current %s)", tag.c_str(), FW_VERSION);
    appLog.add("OTA", lb);
    flashFromRelease(tag);
    return tag;   // reached only if the flash failed
}

bool ReleaseUpdater::fetchLatestTag(String& tagOut) {
    WiFiClientSecure client;
    client.setInsecure();
    client.setBufferSizes(512, 512);   // shrink TLS buffers to keep heap free

    HTTPClient http;
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

    String url = String("https://api.github.com/repos/") + GITHUB_REPO + "/releases/latest";
    if (!http.begin(client, url)) {
        appLog.add("OTA", "check: failed to start request");
        return false;
    }
    http.addHeader("User-Agent", "sofarsolar-mqtt");
    http.addHeader("Accept", "application/vnd.github+json");

    int code = http.GET();
    if (code != HTTP_CODE_OK) {
        char lb[48];
        snprintf(lb, sizeof(lb), "check: GitHub API returned %d", code);
        appLog.add("OTA", lb);
        http.end();
        return false;
    }

    // Only the tag matters; filter avoids buffering the full release payload
    JsonDocument filter;
    filter["tag_name"] = true;

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, http.getStream(),
                                               DeserializationOption::Filter(filter));
    http.end();

    const char* tag = doc["tag_name"];
    if (err || !tag) {
        appLog.add("OTA", "check: failed to parse release JSON");
        return false;
    }

    tagOut = tag;
    return true;
}

bool ReleaseUpdater::flashFromRelease(const String& tag) {
    char hb[64];
    snprintf(hb, sizeof(hb), "OTA: heap free=%u max-block=%u",
             (unsigned)heapStats.freeHeap, (unsigned)heapStats.maxBlock);
    appLog.add("OTA", hb);

    WiFiClientSecure client;
    client.setInsecure();
    client.setBufferSizes(512, 512);   // same as the API check — big default
                                       // TLS record buffers don't fit our heap
    ESP8266HTTPUpdate httpUpdate;
    httpUpdate.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    httpUpdate.rebootOnUpdate(false);

    String url = String("https://github.com/") + GITHUB_REPO +
                 "/releases/download/" + tag + "/firmware.bin";

    appLog.add("OTA", "flashing firmware...");
    t_httpUpdate_return result = httpUpdate.update(client, url, FW_VERSION);
    if (result != HTTP_UPDATE_OK) {
        char lb[96];
        snprintf(lb, sizeof(lb), "OTA failed, error %d: %s",
                 httpUpdate.getLastError(),
                 httpUpdate.getLastErrorString().c_str());
        appLog.add("OTA", lb);
        return false;
    }

    appLog.add("OTA", "update complete, restarting...");
    delay(500);
    ESP.restart();
    return true;
}
