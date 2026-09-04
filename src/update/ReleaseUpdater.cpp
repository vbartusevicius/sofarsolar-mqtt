#include "ReleaseUpdater.h"

#include <memory>
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>
#include <ArduinoJson.h>

#include "util/AppLog.h"
#include "Version.h"

#define RTC_OTA_OFFSET 96            // uint32 block offset in RTC user memory
#define RTC_OTA_MAGIC  0x4F54414CUL

namespace {
struct RtcOtaState {
    uint32_t magic;
    uint32_t attempts;
    char     tag[24];
    uint32_t checksum;
};

uint32_t rtcChecksum(const RtcOtaState& s) {
    uint32_t sum = s.magic ^ (s.attempts * 2654435761UL);
    for (size_t i = 0; i < sizeof(s.tag); i++) sum = sum * 33 + (uint8_t)s.tag[i];
    return sum;
}

bool rtcRead(RtcOtaState& s) {
    if (!ESP.rtcUserMemoryRead(RTC_OTA_OFFSET, (uint32_t*)&s, sizeof(s) / 4)) return false;
    return s.magic == RTC_OTA_MAGIC && s.checksum == rtcChecksum(s);
}

void rtcWrite(RtcOtaState& s) {
    s.magic    = RTC_OTA_MAGIC;
    s.checksum = rtcChecksum(s);
    ESP.rtcUserMemoryWrite(RTC_OTA_OFFSET, (uint32_t*)&s, sizeof(s) / 4);
}

void rtcClear() {
    RtcOtaState s = {};
    ESP.rtcUserMemoryWrite(RTC_OTA_OFFSET, (uint32_t*)&s, sizeof(s) / 4);
}

void logHeap(const char* stage) {
    char lb[72];
    snprintf(lb, sizeof(lb), "%s free=%u max-block=%u",
             stage, (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxFreeBlockSize());
    appLog.add("OTA", lb);
}
} // namespace

void ReleaseUpdater::setCheckIntervalS(uint32_t s) {
    if (s < 60)     s = 60;
    if (s > 86400)  s = 86400;
    _checkIntervalMs = s * 1000UL;
}

void ReleaseUpdater::flashPendingAtBoot() {
    RtcOtaState st;
    if (!rtcRead(st)) return;

    // One attempt per parked release: a second boot with the flag still set
    // means the flash failed, so drop it and boot normally.
    if (st.attempts >= 1) {
        rtcClear();
        appLog.add("OTA", "parked flash failed previously, ignoring");
        return;
    }
    st.attempts = 1;
    rtcWrite(st);

    String tag(st.tag);
    char lb[64];
    snprintf(lb, sizeof(lb), "boot flash of %s starting", tag.c_str());
    appLog.add("OTA", lb);
    logHeap("boot:");

    if (!connectStoredWiFi(30000)) {
        appLog.add("OTA", "boot flash: no WiFi, aborting");
        rtcClear();
        return;
    }

    String url;
    if (resolveAssetUrl(tag, url) && downloadAndFlash(url)) {
        rtcClear();
        appLog.add("OTA", "flash OK, restarting");
        delay(200);
        ESP.restart();
    }
    rtcClear();   // failed — boot normally, next check retries
}

bool ReleaseUpdater::connectStoredWiFi(uint32_t timeoutMs) {
    if (WiFi.status() == WL_CONNECTED) return true;
    WiFi.mode(WIFI_STA);
    WiFi.begin();                      // credentials persisted by the SDK
    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - start) < timeoutMs) {
        delay(100);
    }
    return WiFi.status() == WL_CONNECTED;
}

void ReleaseUpdater::run() {
    long untilDue = (long)(_nextCheckAt - millis());
    if (untilDue > 0) return;
    _nextCheckAt = millis() + _checkIntervalMs;

    if (WiFi.status() != WL_CONNECTED) return;
    checkForUpdates();
}

String ReleaseUpdater::checkNow() {
    _nextCheckAt = millis() + _checkIntervalMs;
    if (WiFi.status() != WL_CONNECTED) return "";
    return checkForUpdates();
}

String ReleaseUpdater::checkForUpdates() {
    String tag;
    if (!fetchLatestTag(tag)) return "";

    if (tag == FW_VERSION) {
        char lb[64];
        snprintf(lb, sizeof(lb), "up to date (%s)", FW_VERSION);
        appLog.add("OTA", lb);
        return "";
    }

    // Park the tag and reboot: the flash needs a clean heap (see header).
    RtcOtaState st = {};
    strncpy(st.tag, tag.c_str(), sizeof(st.tag) - 1);
    st.attempts = 0;
    rtcWrite(st);

    char lb[80];
    snprintf(lb, sizeof(lb), "new release %s (current %s), rebooting to install",
             tag.c_str(), FW_VERSION);
    appLog.add("OTA", lb);
    delay(300);
    ESP.restart();
    return tag;
}

bool ReleaseUpdater::fetchLatestTag(String& tagOut) {
    // Heap-allocated: a WiFiClientSecure is far too large for the 4 KB stack
    std::unique_ptr<WiFiClientSecure> client(new WiFiClientSecure);
    if (!client) return false;
    client->setInsecure();
    client->setBufferSizes(1024, 512);   // API responses are small
    client->setTimeout(15000);

    HTTPClient http;
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.setReuse(false);

    String url = String("https://api.github.com/repos/") + GITHUB_REPO + "/releases/latest";
    if (!http.begin(*client, url)) {
        appLog.add("OTA", "check: begin failed");
        return false;
    }
    http.addHeader("User-Agent", "sofarsolar-mqtt");
    http.addHeader("Accept", "application/vnd.github+json");

    int code = http.GET();
    if (code != HTTP_CODE_OK) {
        char lb[48];
        snprintf(lb, sizeof(lb), "check: HTTP %d", code);
        appLog.add("OTA", lb);
        http.end();
        return false;
    }

    JsonDocument filter;
    filter["tag_name"] = true;
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, http.getStream(),
                                               DeserializationOption::Filter(filter));
    http.end();

    const char* tag = doc["tag_name"];
    if (err || !tag) {
        appLog.add("OTA", "check: bad JSON");
        return false;
    }
    tagOut = tag;
    return true;
}

// github.com 302s to the asset CDN. Resolve it here so the download runs on
// a fresh TLS session — carrying one client across hosts breaks BearSSL.
bool ReleaseUpdater::resolveAssetUrl(const String& tag, String& urlOut) {
    String url = String("https://github.com/") + GITHUB_REPO +
                 "/releases/download/" + tag + "/firmware.bin";

    std::unique_ptr<WiFiClientSecure> client(new WiFiClientSecure);
    if (!client) return false;
    client->setInsecure();
    client->setBufferSizes(1024, 512);
    client->setTimeout(15000);

    HTTPClient http;
    http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
    http.setReuse(false);
    const char* keys[] = { "Location" };
    http.collectHeaders(keys, 1);
    if (!http.begin(*client, url)) {
        appLog.add("OTA", "redirect: begin failed");
        return false;
    }
    http.addHeader("User-Agent", "sofarsolar-mqtt");

    int code = http.GET();
    if (code == HTTP_CODE_FOUND || code == HTTP_CODE_MOVED_PERMANENTLY) {
        urlOut = http.header("Location");
    } else if (code == HTTP_CODE_OK) {
        urlOut = url;
    }
    http.end();

    if (urlOut.length() == 0) {
        char lb[48];
        snprintf(lb, sizeof(lb), "redirect: HTTP %d", code);
        appLog.add("OTA", lb);
        return false;
    }
    return true;
}

bool ReleaseUpdater::downloadAndFlash(const String& url) {
    logHeap("pre-flash:");

    // Default BearSSL buffers (16 KB recv): the asset CDN does not offer
    // MFLN, so a smaller receive buffer would be overrun mid-transfer.
    std::unique_ptr<WiFiClientSecure> client(new WiFiClientSecure);
    if (!client) {
        appLog.add("OTA", "flash: client alloc failed");
        return false;
    }
    client->setInsecure();
    client->setTimeout(30000);

    ESPhttpUpdate.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
    ESPhttpUpdate.rebootOnUpdate(false);
    ESPhttpUpdate.closeConnectionsOnUpdate(true);

    appLog.add("OTA", "downloading firmware...");
    t_httpUpdate_return result = ESPhttpUpdate.update(*client, url, FW_VERSION);
    if (result == HTTP_UPDATE_OK) return true;

    char lb[96];
    snprintf(lb, sizeof(lb), "flash failed %d: %s", ESPhttpUpdate.getLastError(),
             ESPhttpUpdate.getLastErrorString().c_str());
    appLog.add("OTA", lb);
    return false;
}
