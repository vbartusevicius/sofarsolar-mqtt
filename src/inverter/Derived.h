#ifndef SOFAR_INVERTER_DERIVED_H
#define SOFAR_INVERTER_DERIVED_H

#include <stdint.h>
#include "inverter/InverterData.h"

// Values computed from raw registers. Calculated once, here, and published,
// so the LCD, web UI and Home Assistant all show the same number.

// Inverter self-consumption + AC/DC conversion loss in Watts:
// DC input (pvPower - batteryPower) minus AC output (inverterPower), clamped >= 0.
inline int32_t inverterComputeSelfPower(int32_t pvPower, int32_t batteryPower,
                                        int32_t inverterPower)
{
    int32_t loss = pvPower - batteryPower - inverterPower;
    return loss > 0 ? loss : 0;
}

// Share of today's consumption that was not imported from the grid.
inline uint8_t selfSufficiencyPct(float todayUse, float todayImport) {
    if (todayUse <= 0.05f) return 100;
    float own = todayUse - todayImport;
    if (own <= 0) return 0;
    float pct = own * 100.0f / todayUse + 0.5f;
    return pct >= 100.0f ? 100 : (uint8_t)pct;
}

inline const char* runStateName(uint16_t s) {
    static const char* const names[] = {
        "Waiting", "Detection", "Grid-Connected", "EPS Off-Grid",
        "Recoverable Fault", "Permanent Fault", "Upgrading", "Self-Charging",
    };
    return s < sizeof(names) / sizeof(names[0]) ? names[s] : "Unknown";
}

inline const char* workingModeName(uint16_t m) {
    static const char* const names[] = {
        "Self Use", "Time of Use", "Timing", "Passive",
        "Peak Shaving", "Off-Grid", "Generator", "Export Priority",
    };
    return m < sizeof(names) / sizeof(names[0]) ? names[m] : "Unknown";
}

// Called once all three fast blocks are in for this cycle, so self power is
// never computed from one fresh and two stale readings of a failed block.
inline void inverterDeriveFast(InverterData& d) {
    if (!d.fresh()) return;
    d.inverterSelfPower = inverterComputeSelfPower(d.pvPower, d.batteryPower,
                                                   d.inverterPower);
}

inline void inverterDeriveEnergy(InverterData& d) {
    d.selfSufficiency = selfSufficiencyPct(d.todayConsumption, d.todayImport);
}

#endif // SOFAR_INVERTER_DERIVED_H
