#ifndef SOFAR_FLOW_MODEL_H
#define SOFAR_FLOW_MODEL_H

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "inverter/InverterData.h"
#include "inverter/Registers.h"

// What the LCD says, separated from how it is drawn. Pure, so text widths
// are unit-tested against the pixel budget instead of found on the panel.

// Character budgets of the classic 6 px GFX font (size 1) inside each widget
#define FM_CHAR_W        6
#define FM_CARD_W      108
#define FM_HUB_W        88
#define FM_BTN_W       232
#define FM_CARD_CHARS  ((FM_CARD_W - 8) / FM_CHAR_W)        // 16
#define FM_CARD_BIG    ((FM_CARD_W - 8) / (FM_CHAR_W * 2))  // 8 at size 2
#define FM_HUB_CHARS   ((FM_HUB_W - 4) / FM_CHAR_W)         // 14
#define FM_BTN_CHARS   ((FM_BTN_W - 8) / FM_CHAR_W)         // 37
#define FM_SYS_CHARS   ((240 - 8) / FM_CHAR_W)              // 38
#define FM_SYS_LINES     6

// Everything shown that is not inverter data. Built once per redraw by the
// caller, so Display depends on no control or network class.
struct UiStatus {
    bool        wifiOk        = false;
    bool        modbusOk      = false;
    bool        mqttOk        = false;
    bool        saverActive   = false;
    bool        saverNative   = false;
    int32_t     saverMaxPower = 0;
    int32_t     saverTarget   = 0;
    const char* strategyName  = "off";
    const char* modeName      = "auto";
    const char* serial        = "";
    const char* firmware      = "";
    uint32_t    uptimeS       = 0;
    uint8_t     ip[4]         = {0, 0, 0, 0};
    int32_t     rssi          = 0;
    uint32_t    freeHeap      = 0;
    uint32_t    maxBlock      = 0;
    uint8_t     heapFrag      = 0;
};

enum class Tone : uint8_t { Pv, Export, Import, Charge, Discharge, Neutral };

struct CardText {
    const char* title = "";
    char        hdr[14]   = "";
    char        value[16] = "";
    char        row1[20]  = "";
    char        row2[20]  = "";
    int8_t      soc       = -1;    // >= 0 draws the SOC bar instead of row2
    Tone        tone      = Tone::Neutral;
};

struct HubText {
    char line1[16] = "";
    char line2[16] = "";
    bool passiveOk = true;
};

struct ButtonText {
    const char* line1 = "";
    char        line2[40] = "";
    bool        active = false;
};

// Arms show direction only: the magnitude is already in the cards
struct FlowArms {
    bool pv = false;
    bool grid = false, gridToHub = false;
    bool home = false;
    bool batt = false, battToHub = false;
};

inline long fmAbs(long v) { return v < 0 ? -v : v; }

inline bool fmPassiveOk(const InverterData& d) {
    return !d.valid || d.workingMode == WORK_MODE_PASSIVE;
}

inline void flowSolar(const InverterData& d, CardText& c) {
    c.title = "SOLAR";
    c.tone  = Tone::Pv;
    snprintf(c.hdr,   sizeof(c.hdr),   "%.1fkWh", (double)d.todayGeneration);
    snprintf(c.value, sizeof(c.value), "%ld W",   (long)d.pvPower);
    snprintf(c.row1,  sizeof(c.row1),  "PV1: %5ld W", (long)d.pv1Power);
    snprintf(c.row2,  sizeof(c.row2),  "PV2: %5ld W", (long)d.pv2Power);
    c.soc = -1;
}

inline void flowGrid(const InverterData& d, CardText& c) {
    long gw = d.gridPower;
    c.title = "GRID";
    c.tone  = gw >= 0 ? Tone::Export : Tone::Import;
    snprintf(c.hdr, sizeof(c.hdr), "%s", gw > 0 ? "EXPORT" : (gw < 0 ? "IMPORT" : "IDLE"));
    snprintf(c.value, sizeof(c.value), "%ld W", fmAbs(gw));
    snprintf(c.row1, sizeof(c.row1), "%u/%u/%uV",
             (unsigned)(d.gridVoltage   + 0.5f),
             (unsigned)(d.gridVoltageL2 + 0.5f),
             (unsigned)(d.gridVoltageL3 + 0.5f));
    // 99.95 already prints as "100.0": switch to integers before that
    if (d.todayImport >= 99.95f || d.todayExport >= 99.95f)
        snprintf(c.row2, sizeof(c.row2), "In %.0f Out %.0f",
                 (double)d.todayImport, (double)d.todayExport);
    else
        snprintf(c.row2, sizeof(c.row2), "In %.1f Out %.1f",
                 (double)d.todayImport, (double)d.todayExport);
    c.soc = -1;
}

inline void flowHome(const InverterData& d, CardText& c) {
    c.title = "HOME";
    c.tone  = Tone::Neutral;
    snprintf(c.hdr,   sizeof(c.hdr),   "Self %u%%", (unsigned)d.selfSufficiency);
    snprintf(c.value, sizeof(c.value), "%ld W",     (long)d.loadPower);
    snprintf(c.row1,  sizeof(c.row1),  "Today %.1f kWh", (double)d.todayConsumption);
    c.row2[0] = '\0';
    c.soc = -1;
}

inline void flowBatt(const InverterData& d, CardText& c) {
    long bw = d.batteryPower;
    c.title = "BATT";
    c.tone  = bw >= 0 ? Tone::Charge : Tone::Discharge;
    snprintf(c.hdr, sizeof(c.hdr), "%s", bw > 0 ? "CHG" : (bw < 0 ? "DIS" : "IDLE"));
    snprintf(c.value, sizeof(c.value), "%ld W", fmAbs(bw));
    snprintf(c.row1, sizeof(c.row1), "SOC %u%%   %dC",
             (unsigned)(d.batterySOC > 100 ? 100 : d.batterySOC), (int)d.battTemp);
    c.row2[0] = '\0';
    c.soc = (int8_t)(d.batterySOC > 100 ? 100 : d.batterySOC);
}

inline void flowHub(const InverterData& d, HubText& h) {
    snprintf(h.line1, sizeof(h.line1), "INV %dC", (int)d.inverterTemp);
    snprintf(h.line2, sizeof(h.line2), "%ld W",   (long)d.inverterSelfPower);
    h.passiveOk = fmPassiveOk(d);
}

inline void flowArms(const InverterData& d, FlowArms& a) {
    a.pv        = d.pvPower > 0;
    a.grid      = d.gridPower != 0;
    a.gridToHub = d.gridPower < 0;
    a.home      = d.loadPower > 0;
    a.batt      = d.batteryPower != 0;
    a.battToHub = d.batteryPower < 0;
}

inline void flowButton(const InverterData& d, const UiStatus& s, ButtonText& b) {
    b.active = s.saverActive;
    b.line1  = s.saverActive ? "BATT SAVE  ON" : "BATT SAVE OFF";
    if (!fmPassiveOk(d))
        snprintf(b.line2, sizeof(b.line2), "WARN: NOT IN PASSIVE (mode %u)",
                 (unsigned)d.workingMode);
    else if (s.saverActive && s.saverNative)
        snprintf(b.line2, sizeof(b.line2), "NATIVE [0..%ldkW]  PASSIVE",
                 (long)(s.saverMaxPower / 1000));
    else if (s.saverActive)
        snprintf(b.line2, sizeof(b.line2), "LOOP Target: %ld W", (long)s.saverTarget);
    else
        snprintf(b.line2, sizeof(b.line2), "Mode: %s  (PASSIVE)", s.modeName);
}

inline void sysLines(const InverterData& d, const UiStatus& s,
                     char out[FM_SYS_LINES][64])
{
    snprintf(out[0], 64, "FW: %s", s.firmware);
    snprintf(out[1], 64, "SN: %s", s.serial);
    uint32_t up = s.uptimeS;
    snprintf(out[2], 64, "IP: %u.%u.%u.%u  Up: %lud %02lu:%02lu",
             s.ip[0], s.ip[1], s.ip[2], s.ip[3],
             (unsigned long)(up / 86400), (unsigned long)((up / 3600) % 24),
             (unsigned long)((up / 60) % 60));
    snprintf(out[3], 64, "RSSI: %ld dBm   Heap: %u/%u kB (%u%%)",
             (long)s.rssi, (unsigned)(s.freeHeap / 1024),
             (unsigned)(s.maxBlock / 1024), (unsigned)s.heapFrag);
    snprintf(out[4], 64, "Temp I/H/B: %d/%d/%dC  Self: %ldW",
             (int)d.inverterTemp, (int)d.heatsinkTemp, (int)d.battTemp,
             (long)d.inverterSelfPower);
    snprintf(out[5], 64, "Mode: %s (%s)  Inv: %u",
             s.modeName, s.strategyName, (unsigned)d.workingMode);
}

#endif // SOFAR_FLOW_MODEL_H
