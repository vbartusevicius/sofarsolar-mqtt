#ifndef SOFAR_MODE_H
#define SOFAR_MODE_H

#include <stdint.h>
#include <string.h>

// Control modes. Text only exists at the edges (MQTT, web, EEPROM, LCD);
// everything inside compares the enum. Names are the persisted EEPROM format
// and the MQTT/HA select options — do not rename them.
enum class Mode : uint8_t { Auto = 0, Charge, Standby, BatterySaver };

inline const char* modeName(Mode m) {
    switch (m) {
    case Mode::Auto:         return "auto";
    case Mode::Charge:       return "charge";
    case Mode::Standby:      return "standby";
    case Mode::BatterySaver: return "battery_saver";
    }
    return "auto";
}

// Exact match only: "chargeX" or "stop" are rejected, not guessed.
inline bool modeParse(const char* s, Mode& out) {
    if (!s) return false;
    static const Mode all[] = { Mode::Auto, Mode::Charge, Mode::Standby,
                                Mode::BatterySaver };
    for (Mode m : all) {
        if (strcmp(s, modeName(m)) == 0) { out = m; return true; }
    }
    return false;
}

// The one rule for "battery saver off", wherever it comes from (LCD button,
// web toggle, Home Assistant switch): hand the battery back to the inverter's
// normal self-use behaviour. Turning it off while another mode is active
// leaves that mode alone.
inline Mode modeForSaverSwitch(Mode current, bool on) {
    if (on) return Mode::BatterySaver;
    return current == Mode::BatterySaver ? Mode::Auto : current;
}

// Maps a static mode to its passive power range [outMin, outMax].
// Returns false for BatterySaver, which computes its own range.
inline bool modeDesiredPassiveRange(Mode m, int32_t chargePower,
                                    int32_t autoLimit,
                                    int32_t& outMin, int32_t& outMax)
{
    switch (m) {
    case Mode::Charge:
        outMin = outMax = chargePower;
        return true;
    case Mode::Standby:
        outMin = outMax = 0;
        return true;
    case Mode::Auto:
        outMin = -autoLimit;
        outMax = autoLimit;
        return true;
    case Mode::BatterySaver:
        return false;
    }
    return false;
}

#endif // SOFAR_MODE_H
