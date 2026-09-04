#include "ReleaseUpdater.h"

#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>
#include <ArduinoJson.h>

#include "util/AppLog.h"
#include "util/HeapStats.h"
#include "Version.h"

#define RTC_OTA_OFFSET 96            // uint32 block offset in RTC user mem
#define RTC_OTA_MAGIC  0x4F54414CUL  // "OTAL"

namespace {
struct RtcOtaState {
    uint32_t magic;
    uint32_t attempts;
    char     tag[24];
    uint32_t checksum;
};

uint32_t rtcChecksum(const RtcOtaState& s) {
    uint32_t sum = s.magic ^ s.attempts;
    for (size_t i = 0; i < sizeof(s.tag); i++) sum = sum * 33 + (uint8_t)s.tag[i];
    return sum;
}

bool rtcRead(RtcOtaState& s) {
    if (!ESP.rtcUserMemoryRead(RTC_OTA_OFFSET, (uint32_t*)&s, sizeof(s))) return false;
    return s.magic == RTC_OTA_MAGIC && s.checksum == rtcChecksum(s);
}

void rtcWrite(RtcOtaState& s) {
    s.magic    = RTC_OTA_MAGIC;
    s.checksum = rtcChecksum(s);
    ESP.rtcUserMemoryWrite(RTC_OTA_OFFSET, (uint32_t*)&s, sizeof(s));
}

void rtcClear() {
    RtcOtaState s = {};           // magic = 0 → invalid
    ESP.rtcUserMemoryWrite(RTC_OTA_OFFSET, (uint32_t*)&s, sizeof(s));
}
} // namespace

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

    if (tag == _failedTag)                    // already tried and failed — don’t
        return "check skipped";               // schedule another flash cycle

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
    _busy = true;   // periodic tasks pause while we download/flash

    char hb[64];
    snprintf(hb, sizeof(hb), "OTA: heap free=%u max-block=%u",
             (unsigned)heapStats.freeHeap, (unsigned)heapStats.maxBlock);
    appLog.add("OTA", hb);

    String url = String("https://github.com/") + GITHUB_REPO +
                 "/releases/download/" + tag + "/firmware.bin";

    // Step 1: resolve the asset redirect ourselves.  github.com 302s to
    // objects.githubusercontent.com; letting HTTPClient carry the SAME
    // WiFiClientSecure across hosts corrupts the BearSSL session and the
    // server drops us mid-transfer (error -5 "connection lost").
    String downloadUrl;
    {
        WiFiClientSecure probe;
        probe.setInsecure();
        probe.setBufferSizes(512, 512);
        probe.setTimeout(10000);
        HTTPClient http;
        http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
        const char* keys[] = { "Location" };
        http.collectHeaders(keys, 1);
        if (!http.begin(probe, url)) {
            appLog.add("OTA", "redirect probe: begin failed");
            _busy = false;
            return false;
        }
        http.addHeader("User-Agent", "sofarsolar-mqtt");
        int code = http.GET();
        if (code == HTTP_CODE_FOUND || code == HTTP_CODE_MOVED_PERMANENTLY) {
            downloadUrl = http.header("Location");
        } else if (code == HTTP_CODE_OK) {
            downloadUrl = url;
        }
        http.end();
        if (downloadUrl.length() == 0) {
            snprintf(hb, sizeof(hb), "redirect probe: HTTP %d", code);
            appLog.add("OTA", hb);
            _busy = false;
            return false;
        }
    }
    appLog.add("OTA", "redirect resolved, downloading...");

    // Step 2: fresh client, direct download, no further redirects.
    // TLS buffer policy: small buffers are only legal if the server honours
    // MFLN (RFC6066).  GitHub's asset CDN may not — a full-size TLS record
    // into a small BearSSL buffer kills the connection mid-transfer
    // (error -5).  Probe first; fall back to full buffers if heap allows.
    String host = downloadUrl;
    int hs = host.indexOf("://");
    if (hs >= 0) host.remove(0, hs + 3);
    int pe = host.indexOf('/');
    if (pe >= 0) host.remove(pe);

    t_httpUpdate_return result;
    _teardownUsed = false;
    {
        WiFiClientSecure client;
        client.setInsecure();
        client.setTimeout(30000);           // slow TLS + 500 kB payload

        bool mfln = client.probeMaxFragmentLength(host.c_str(), 443, 1024);
        if (mfln) {
            client.setBufferSizes(1024, 512);
            appLog.add("OTA", "server supports MFLN (small TLS buffers)");
        } else {
            uint32_t blk = ESP.getMaxFreeBlockSize();
            if (blk < 26 * 1024) {
                // Try to recover heap live — no reboot needed if the
                // teardown frees enough contiguous memory.
                if (_teardownHook) {
                    appLog.add("OTA", "heap too small, tearing down network services");
                    _teardownHook();
                    _teardownUsed = true;
                    delay(100);
                    blk = ESP.getMaxFreeBlockSize();
                    snprintf(hb, sizeof(hb), "OTA: post-teardown max-block=%u", (unsigned)blk);
                    appLog.add("OTA", hb);
                }
                if (blk < 26 * 1024) {
                    RtcOtaState st = {};
                    bool pending = rtcRead(st);
                    if (pending && st.attempts >= 1) {
                        // Boot-time flash failed once already for this tag —
                        // never boot-loop on a release.
                        appLog.add("OTA", "boot flash failed again, giving up on this release");
                        rtcClear();
                        _failedTag = tag;
                        _busy = false;
                        if (_teardownUsed && _resumeHook) _resumeHook();
                        return false;
                    }
                    if (!pending) {          // keep attempts from the boot path
                        st.attempts = 0;
                        strncpy(st.tag, tag.c_str(), sizeof(st.tag) - 1);
                        rtcWrite(st);
                    }
                    appLog.add("OTA", "still too small — rebooting to flash clean");
                    _busy = false;
                    delay(200);
                    ESP.restart();
                }
            }
            appLog.add("OTA", "no MFLN, using full TLS buffers");
        }

        ESP8266HTTPUpdate httpUpdate;
        httpUpdate.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
        httpUpdate.rebootOnUpdate(false);

        appLog.add("OTA", "flashing firmware...");
        result = httpUpdate.update(client, downloadUrl, FW_VERSION);
    }
    _busy = false;

    if (result != HTTP_UPDATE_OK) {
        ESP8266HTTPUpdate httpUpdate;   // reuse for error strings only
        char lb[96];
        snprintf(lb, sizeof(lb), "OTA failed, error %d: %s",
                 httpUpdate.getLastError(),
                 httpUpdate.getLastErrorString().c_str());
        appLog.add("OTA", lb);
        if (_teardownUsed && _resumeHook) _resumeHook();
        return false;
    }

    appLog.add("OTA", "update complete, restarting...");
    delay(500);
    ESP.restart();
    return true;
}

// ── Pending boot-time flash ────────────────────────────────────
bool ReleaseUpdater::hasPendingFlash() const {
    RtcOtaState st;
    return rtcRead(st);
}

void ReleaseUpdater::maybeFlashPending() {
    RtcOtaState st;
    if (!rtcRead(st)) return;

    if (st.attempts >= 1) {
        rtcClear();
        appLog.add("OTA", "pending flash done/aborted, normal boot");
        return;
    }

    appLog.add("OTA", "pending flash: boot-time update starting");
    st.attempts++;
    rtcWrite(st);

    String tag(st.tag);
    flashFromRelease(tag);

    rtcClear();
    _failedTag = tag;        // don’t let the next runtime check re-arm it
}
