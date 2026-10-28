#ifndef SOFAR_LOOP_WATCHDOG_H
#define SOFAR_LOOP_WATCHDOG_H

#include <stdint.h>

// The hardware watchdog only fires when nothing yields for ~8 s. A call that
// blocks *while yielding* (a TCP write into a stalled socket, a DNS lookup)
// feeds it forever: the device looks dead — no web, no MQTT, no touch wake —
// yet never resets. This supervisor runs from a timer, which still fires
// during those yields, and restarts the device when loop() stops completing.

#define LOOP_STALL_MS   120000UL   // > worst legitimate stall (web firmware upload)
#define LOOP_WD_MAGIC   0x534C5754UL
#define LOOP_WD_RTC_OFS 64         // RTC user blocks 0..31 belong to OTA/eboot

enum LoopStage : uint8_t {
    STAGE_LOOP = 0,
    STAGE_POLL,
    STAGE_BSAVE,
    STAGE_MQTT_PUB,
    STAGE_MQTT_CONN,
    STAGE_DISPLAY,
    STAGE_HEALTH,
    STAGE_PASSIVE_TMO,
    STAGE_WIFI,
    STAGE_OTA,
    STAGE_MDNS,
    STAGE_WEB,
    STAGE_MQTT_LOOP,
    STAGE_TOUCH,
    STAGE_COUNT
};

inline const char* loopStageName(uint8_t s) {
    static const char* const names[STAGE_COUNT] = {
        "loop", "modbus poll", "battery saver", "mqtt publish",
        "mqtt connect", "display", "health", "passive timeout",
        "wifi", "ota", "mdns", "web", "mqtt loop", "touch",
    };
    return s < STAGE_COUNT ? names[s] : "unknown";
}

inline bool loopStalled(uint32_t now, uint32_t lastBeat, uint32_t limitMs) {
    return (uint32_t)(now - lastBeat) >= limitMs;
}

// Survives a soft reset in RTC memory (not a power cycle). `stalled` is set by
// the supervisor; otherwise the record is the last stage snapshot, which
// explains a hardware-watchdog or exception reset after the fact.
struct LoopWdRecord {
    uint32_t magic;
    uint8_t  stage;
    uint8_t  stalled;
    uint16_t reserved;
    uint32_t uptimeS;
    uint32_t check;
};

inline uint32_t loopWdCheck(const LoopWdRecord& r) {
    return r.magic ^ ((uint32_t)r.stage << 8) ^ ((uint32_t)r.stalled << 16) ^
           r.uptimeS ^ 0xA5A5A5A5UL;
}

inline void loopWdSeal(LoopWdRecord& r, uint8_t stage, bool stalled,
                       uint32_t uptimeS)
{
    r.magic    = LOOP_WD_MAGIC;
    r.stage    = stage;
    r.stalled  = stalled ? 1 : 0;
    r.reserved = 0;
    r.uptimeS  = uptimeS;
    r.check    = loopWdCheck(r);
}

inline bool loopWdValid(const LoopWdRecord& r) {
    return r.magic == LOOP_WD_MAGIC && r.stage < STAGE_COUNT &&
           r.check == loopWdCheck(r);
}

#endif // SOFAR_LOOP_WATCHDOG_H
