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
// Progress breadcrumbs survive the install reboot, so a failed attempt can
// still be explained afterwards (the RAM log is wiped by the restart).
enum : uint16_t {
    ST_NONE = 0, ST_PARKED, ST_BOOT_START, ST_WIFI_FAIL,
    ST_REDIRECT_FAIL, ST_DOWNLOADING, ST_FLASH_FAIL, ST_DONE, ST_GAVE_UP
};

const char* stageName(uint16_t s) {
    switch (s) {
        case ST_PARKED:        return "parked, awaiting reboot";
        case ST_BOOT_START:    return "boot flash started";
        case ST_WIFI_FAIL:     return "no WiFi at boot";
        case ST_REDIRECT_FAIL: return "asset URL not resolved";
        case ST_DOWNLOADING:   return "download/flash in progress";
        case ST_FLASH_FAIL:    return "download/flash failed";
        case ST_DONE:          return "installed OK";
        case ST_GAVE_UP:       return "gave up after one attempt";
        default:               return "none";
    }
}

struct RtcOtaState {
    uint32_t magic;
    uint8_t  pending;
    uint8_t  attempts;
    uint16_t stage;
    int32_t  err;
    uint32_t freeHeap;
    uint32_t maxBlock;
    char     tag[24];
    uint32_t checksum;
};
// system_rtc_mem_* requires a size that is a multiple of 4 bytes
static_assert(sizeof(RtcOtaState) % 4 == 0, "RTC state must be 4-byte sized");

uint32_t rtcChecksum(const RtcOtaState& s) {
    uint32_t sum = s.magic ^ (s.pending * 31u) ^ (s.attempts * 131u)
                 ^ (s.stage * 7919u) ^ (uint32_t)s.err
                 ^ s.freeHeap ^ s.maxBlock;
    for (size_t i = 0; i < sizeof(s.tag); i++) sum = sum * 33 + (uint8_t)s.tag[i];
    return sum;
}

bool rtcRead(RtcOtaState& s) {
    // NOTE: size is in BYTES (offset is in 4-byte blocks)
    if (!ESP.rtcUserMemoryRead(RTC_OTA_OFFSET, (uint32_t*)&s, sizeof(s))) return false;
    return s.magic == RTC_OTA_MAGIC && s.checksum == rtcChecksum(s);
}

void rtcWrite(RtcOtaState& s) {
    s.magic    = RTC_OTA_MAGIC;
    s.checksum = rtcChecksum(s);
    ESP.rtcUserMemoryWrite(RTC_OTA_OFFSET, (uint32_t*)&s, sizeof(s));
}

void rtcTrace(RtcOtaState& s, uint16_t stage, int32_t err = 0) {
    s.stage    = stage;
    s.err      = err;
    s.freeHeap = ESP.getFreeHeap();
    s.maxBlock = ESP.getMaxFreeBlockSize();
    rtcWrite(s);

    char lb[96];
    snprintf(lb, sizeof(lb), "%s (err=%ld free=%u blk=%u)", stageName(stage),
             (long)err, (unsigned)s.freeHeap, (unsigned)s.maxBlock);
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
    if (!rtcRead(st) || !st.pending) return;

    String tag(st.tag);
    char lb[72];
    snprintf(lb, sizeof(lb), "installing %s (current %s)", tag.c_str(), FW_VERSION);
    appLog.add("OTA", lb);

    // One attempt per parked release: a second boot with the flag still set
    // means the flash died mid-way, so stop trying.
    if (st.attempts >= 1) {
        st.pending = 0;
        rtcTrace(st, ST_GAVE_UP);
        return;
    }
    st.attempts = 1;
    rtcTrace(st, ST_BOOT_START);

    if (!connectStoredWiFi(30000)) {
        st.pending = 0;
        rtcTrace(st, ST_WIFI_FAIL);
        return;
    }

    String url;
    if (!resolveAssetUrl(tag, url)) {
        st.pending = 0;
        rtcTrace(st, ST_REDIRECT_FAIL);
        return;
    }

    rtcTrace(st, ST_DOWNLOADING);
    if (downloadAndFlash(url)) {
        st.pending = 0;
        rtcTrace(st, ST_DONE);
        delay(200);
        ESP.restart();
    }
    st.pending = 0;
    rtcTrace(st, ST_FLASH_FAIL, _lastError);
}

// Emitted once per boot so the web log always shows the previous outcome
void ReleaseUpdater::logLastAttempt() {
    RtcOtaState st;
    if (!rtcRead(st) || st.stage == ST_NONE) return;
    char lb[120];
    snprintf(lb, sizeof(lb), "last attempt: %s [%s] err=%ld free=%u blk=%u",
             st.tag, stageName(st.stage), (long)st.err,
             (unsigned)st.freeHeap, (unsigned)st.maxBlock);
    appLog.add("OTA", lb);
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
    st.pending  = 1;
    st.attempts = 0;

    char lb[80];
    snprintf(lb, sizeof(lb), "new release %s (current %s), rebooting to install",
             tag.c_str(), FW_VERSION);
    appLog.add("OTA", lb);
    rtcTrace(st, ST_PARKED);
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
    // Default BearSSL buffers (16 KB recv): the asset CDN does not offer
    // MFLN, so a smaller receive buffer would be overrun mid-transfer.
    std::unique_ptr<WiFiClientSecure> client(new WiFiClientSecure);
    if (!client) {
        _lastError = -100;
        appLog.add("OTA", "flash: client alloc failed");
        return false;
    }
    client->setInsecure();
    client->setTimeout(30000);

    ESPhttpUpdate.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
    ESPhttpUpdate.rebootOnUpdate(false);
    ESPhttpUpdate.closeConnectionsOnUpdate(true);

    t_httpUpdate_return result = ESPhttpUpdate.update(*client, url, FW_VERSION);
    if (result == HTTP_UPDATE_OK) return true;

    _lastError = ESPhttpUpdate.getLastError();
    char lb[112];
    snprintf(lb, sizeof(lb), "flash failed %ld: %s", (long)_lastError,
             ESPhttpUpdate.getLastErrorString().c_str());
    appLog.add("OTA", lb);
    return false;
}
