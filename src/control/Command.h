#ifndef SOFAR_COMMAND_H
#define SOFAR_COMMAND_H

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "control/Mode.h"
#include "Tuning.h"

// One parser for every control surface. MQTT topics "<device>/set/<key>" and
// web endpoints "/api/<key>?v=<value>" share the same keys, so the two can
// never drift apart in what a command means.
enum class CmdKind : uint8_t { SetMode, SaverSwitch, SaverToggle, SetCharge, SetAuto };

struct Command {
    CmdKind kind  = CmdKind::SetMode;
    Mode    mode  = Mode::Auto;
    bool    on    = false;
    int32_t value = 0;
};

inline bool cmdParseInt(const char* s, int32_t lo, int32_t hi, int32_t& out) {
    if (!s || !*s) return false;
    char* end = nullptr;
    long v = strtol(s, &end, 10);
    if (end == s || *end != '\0') return false;
    out = v < lo ? lo : (v > hi ? hi : (int32_t)v);
    return true;
}

inline bool cmdParseBool(const char* s, bool& out) {
    if (!s) return false;
    if (!strcmp(s, "on") || !strcmp(s, "true") || !strcmp(s, "1")  || !strcmp(s, "ON"))  { out = true;  return true; }
    if (!strcmp(s, "off")|| !strcmp(s, "false")|| !strcmp(s, "0")  || !strcmp(s, "OFF")) { out = false; return true; }
    return false;
}

// key: "mode" | "battery_save" | "charge" | "auto" | "standby"
// value: payload / query value; empty means "no value" (toggle for battery_save)
inline bool parseCommand(const char* key, const char* value, Command& out) {
    if (!key) return false;
    if (!value) value = "";
    out = Command();
    if (!strcmp(key, "battery_save")) {
        if (!*value) { out.kind = CmdKind::SaverToggle; return true; }
        out.kind = CmdKind::SaverSwitch;
        return cmdParseBool(value, out.on);
    }
    if (!strcmp(key, "mode")) {
        out.kind = CmdKind::SetMode;
        return modeParse(value, out.mode);
    }
    if (!strcmp(key, "standby")) {
        out.kind = CmdKind::SetMode;
        out.mode = Mode::Standby;
        return true;
    }
    if (!strcmp(key, "charge")) {
        out.kind = CmdKind::SetCharge;
        return cmdParseInt(value, -BSAVE_MAX_POWER, BSAVE_MAX_POWER, out.value);
    }
    if (!strcmp(key, "auto")) {
        out.kind = CmdKind::SetAuto;
        if (!*value) { out.value = AUTO_LIMIT_DEFAULT; return true; }
        return cmdParseInt(value, 100, BSAVE_MAX_POWER, out.value);
    }
    return false;
}

// "<device>/set/<key>" -> "<key>", or nullptr for any other topic.
inline const char* mqttCommandKey(const char* topic) {
    if (!topic) return nullptr;
    const char* p = strstr(topic, "/set/");
    return p && p[5] ? p + 5 : nullptr;
}

#endif // SOFAR_COMMAND_H
