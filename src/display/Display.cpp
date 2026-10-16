#include "Display.h"
#include <ESP8266WiFi.h>
#include <Fonts/Picopixel.h>
#include "display/TouchGesture.h"
#include "Config.h"
#include "inverter/Inverter.h"
#include "control/BatterySaver.h"
#include "control/ModeController.h"
#include "util/AppLog.h"
#include "util/HeapStats.h"
#include "Version.h"

#define CLR_BG       ILI9341_BLACK
#define CLR_PANEL    ILI9341_BLACK     // panels are outline-only for contrast
#define CLR_LABEL    0xE71C            // near-white grey
#define CLR_EXPORT   ILI9341_GREEN
#define CLR_IMPORT   0xFA8C            // light salmon, readable on black
#define CLR_CHARGE   0x3D7F            // light sky blue
#define CLR_DISCHG   ILI9341_ORANGE
#define CLR_PV       ILI9341_YELLOW
#define CLR_TARGET   ILI9341_CYAN
#define CLR_ACTIVE   ILI9341_GREEN
#define CLR_OFF      0x8410            // dim grey

#define BL_FULL      255   // ESP8266 analogWrite range is 255: 32 was 12% brightness

#define CHAR_W         6   // classic GFX font is 6 px wide at size 1

#define TAB_H         28
#define TAB_FLOW_X1   86
#define TAB_SYS_X1   172

// 2x2 Quadrant Card geometry + Central INV Hub (240x320 portrait)
#define CD_W         108
#define CD_H          82
#define CD_LX          4
#define CD_RX        128
#define CD_TY         32
#define CD_BY        172

#define HUB_X         76
#define HUB_Y        123
#define HUB_W         88
#define HUB_H         40

#define BTN_Y        260
#define BTN_H         42
#define HINT_Y       311
#define LOG_ROW_H      7   // Picopixel yAdvance
#define LOG_BASELINE   5   // glyph height above the baseline
#define LOG_COLS      60   // 232 px / ~4 px per Picopixel char, + NUL
#define BTN_X          4
#define BTN_W        232

// Touch is gesture-based, not coordinate-based: panel wiring/rotation varies
// between board revisions and cannot be calibrated blind.
//   short tap  = toggle battery saver (FLOW tab)
//   long press = switch tab
#define TS_POLL_MS     30
#define TS_LONG_MS    800
#define TS_CAL_MS    4000   // hold this long to start touch calibration
#define TS_MIN_TAP_MS  30
#define TS_RELEASE_MS 250

Display::Display()
    : _tft(PIN_TFT_CS, PIN_TFT_DC),
      _ts(PIN_TOUCH_CS, PIN_TOUCH_IRQ)
{}

void Display::begin() {
    _tft.begin();
    _tft.setRotation(2);             // portrait, USB at top
    _ts.begin();
    _ts.setRotation(1);
    analogWrite(PIN_TFT_LED, BL_FULL);
    _brightness = BL_FULL;
    _screenOn   = true;
    _lastTouch  = millis();
    _needFullDraw = true;
}

void Display::showSplash(const char* line1, const char* line2) {
    _tft.fillScreen(CLR_BG);
    drawCentered(120, String(line1), 2, ILI9341_WHITE);
    drawCentered(150, String(line2), 2, CLR_PV);
}


void Display::drawCentered(int16_t y, const String& text, uint8_t size,
                           uint16_t fg)
{
    int16_t w = text.length() * CHAR_W * size;
    _tft.setTextSize(size);
    _tft.setTextColor(fg, CLR_BG);
    _tft.setCursor((240 - w) / 2, y);
    _tft.print(text);
}

// Erase the previous string exactly by reprinting it in the background
// colour, then print the new one — no fillRect "wipe" frame.
void Display::printOver(int16_t x0, int16_t x1, int16_t y, uint8_t size,
                        uint16_t fg, uint16_t bg,
                        const char* prev, const char* next)
{
    _tft.setTextSize(size);
    _tft.setCursor(x0, y);
    _tft.setTextColor(bg, bg);
    _tft.print(prev);
    _tft.setCursor(x1, y);
    _tft.setTextColor(fg, bg);
    _tft.print(next);
}

void Display::drawHintLine(const char* text) {
    _tft.fillRect(0, HINT_Y, 240, 8, CLR_BG);
    _tft.setTextSize(1);
    _tft.setTextColor(CLR_OFF, CLR_BG);
    _tft.setCursor((240 - (int16_t)strlen(text) * CHAR_W) / 2, HINT_Y);
    _tft.print(text);
}

void Display::drawHint() {
    if (_calNotice) { drawHintLine("keep holding for calibration..."); return; }
    if (_cal.valid) drawHintLine("tap tabs/button   hold: switch tab");
    else drawHintLine(_tab == TAB_FLOW ? "tap: batt save  hold: switch tab"
                                       : "hold anywhere: switch tab");
}

void Display::drawTabBar() {
    const uint16_t barBg    = 0x0841;   // dim grey-blue
    const uint16_t activeBg = 0x0010;   // dark blue
    _tft.fillRect(0, 0, 240, TAB_H, barBg);
    for (int i = 0; i < 2; i++) {
        int16_t x = i * TAB_FLOW_X1;
        bool active = ((int)_tab == i);
        if (active) _tft.fillRect(x, 0, TAB_FLOW_X1, TAB_H, activeBg);
        const char* label = i == 0 ? "FLOW" : "SYS";
        _tft.setTextSize(2);
        _tft.setTextColor(active ? ILI9341_WHITE : 0xC618,
                          active ? activeBg : barBg);
        int16_t tw = strlen(label) * CHAR_W * 2;
        _tft.setCursor(x + (TAB_FLOW_X1 - tw) / 2, 5);
        _tft.print(label);
        if (active)
            _tft.fillRect(x, TAB_H - 3, TAB_FLOW_X1, 3, CLR_PV);
        _tft.drawFastVLine(x + TAB_FLOW_X1, 4, TAB_H - 8, CLR_OFF);
    }
    _tft.drawFastHLine(0, TAB_H, 240, CLR_OFF);
    _dotsValid = false;
}

void Display::drawTopLinkDots(bool wifiOk, bool modbusOk, bool mqttOk) {
    uint8_t state = (wifiOk ? 1 : 0) | (modbusOk ? 2 : 0) | (mqttOk ? 4 : 0);
    if (_dotsValid && _dotsCache == state) return;
    _dotsValid = true;
    _dotsCache = state;

    const uint16_t barBg = 0x0841;
    const char labels[3] = { 'W', 'R', 'M' };
    const bool ok[3]     = { wifiOk, modbusOk, mqttOk };
    for (int i = 0; i < 3; i++) {
        int16_t cx = TAB_SYS_X1 + 12 + i * 21;
        _tft.fillCircle(cx, 10, 4, ok[i] ? CLR_ACTIVE : CLR_IMPORT);
        _tft.setTextSize(1);
        _tft.setTextColor(CLR_LABEL, barBg);
        _tft.setCursor(cx - 2, 18);
        _tft.print(labels[i]);
    }
}

void Display::redrawCard(int16_t x, int16_t y, int16_t w, int16_t h,
                         const char* title, const char* hdrRight,
                         const char* value, const char* row1, const char* row2,
                         int8_t socPct, uint16_t color, CardCache& c)
{
    if (!c.valid || c.color != color) {
        _tft.fillRect(x, y, w, h, CLR_BG);
        _tft.drawRect(x, y, w, h, color);
        _tft.drawFastHLine(x + 2, y + 14, w - 4, 0x2104);
        _tft.setTextSize(1);
        _tft.setTextColor(CLR_LABEL, CLR_PANEL);
        _tft.setCursor(x + 4, y + 4);
        _tft.print(title);
        c.valid       = true;
        c.color       = color;
        c.hdrRight[0] = c.value[0] = c.row1[0] = c.row2[0] = '\0';
        c.socPct      = -1;
    }

    if (strcmp(c.hdrRight, hdrRight) != 0) {
        int16_t ox = x + w - 4 - (int16_t)strlen(c.hdrRight) * CHAR_W;
        int16_t nx = x + w - 4 - (int16_t)strlen(hdrRight)   * CHAR_W;
        printOver(ox, nx, y + 4, 1, color, CLR_PANEL, c.hdrRight, hdrRight);
        strncpy(c.hdrRight, hdrRight, sizeof(c.hdrRight) - 1);
        c.hdrRight[sizeof(c.hdrRight) - 1] = '\0';
    }

    int16_t valY = (row2[0] == '\0' && socPct < 0) ? (y + 24) : (y + 19);
    if (strcmp(c.value, value) != 0) {
        int16_t ox = x + (w - (int16_t)strlen(c.value) * CHAR_W * 2) / 2;
        int16_t nx = x + (w - (int16_t)strlen(value)   * CHAR_W * 2) / 2;
        printOver(ox, nx, valY, 2, color, CLR_PANEL, c.value, value);
        strncpy(c.value, value, sizeof(c.value) - 1);
        c.value[sizeof(c.value) - 1] = '\0';
    }

    int16_t r1Y = (row2[0] == '\0' && socPct < 0) ? (y + 54) : (y + 43);
    if (strcmp(c.row1, row1) != 0) {
        int16_t ox = x + (w - (int16_t)strlen(c.row1) * CHAR_W) / 2;
        int16_t nx = x + (w - (int16_t)strlen(row1)   * CHAR_W) / 2;
        printOver(ox, nx, r1Y, 1, CLR_LABEL, CLR_PANEL, c.row1, row1);
        strncpy(c.row1, row1, sizeof(c.row1) - 1);
        c.row1[sizeof(c.row1) - 1] = '\0';
    }

    if (socPct >= 0) {
        if (c.socPct != socPct) {
            c.socPct = socPct;
            int16_t bx = x + 6, by = y + 58, bw = w - 12, bh = 14;
            _tft.drawRect(bx, by, bw, bh, CLR_LABEL);
            int16_t innerMax = bw - 4;
            int16_t fillW    = (int16_t)((int32_t)innerMax * socPct / 100);
            uint16_t barClr  = socPct >= 50 ? CLR_ACTIVE
                             : (socPct >= 20 ? CLR_PV : CLR_IMPORT);
            if (fillW > 0)
                _tft.fillRect(bx + 2, by + 2, fillW, bh - 4, barClr);
            if (fillW < innerMax)
                _tft.fillRect(bx + 2 + fillW, by + 2, innerMax - fillW, bh - 4, CLR_BG);
        }
    } else if (strcmp(c.row2, row2) != 0) {
        int16_t ox = x + (w - (int16_t)strlen(c.row2) * CHAR_W) / 2;
        int16_t nx = x + (w - (int16_t)strlen(row2)   * CHAR_W) / 2;
        printOver(ox, nx, y + 58, 1, CLR_LABEL, CLR_PANEL, c.row2, row2);
        strncpy(c.row2, row2, sizeof(c.row2) - 1);
        c.row2[sizeof(c.row2) - 1] = '\0';
    }
}

void Display::redrawHub(const InverterData& inv, bool passiveOk) {
    uint16_t border = passiveOk ? CLR_TARGET : CLR_IMPORT;
    char l1[16], l2[16];
    snprintf(l1, sizeof(l1), "INV %dC", (int)inv.inverterTemp);
    snprintf(l2, sizeof(l2), "%ld W",   (long)inv.inverterSelfPower);

    if (!_hubInv.valid || _hubInv.color != border) {
        _tft.fillRect(HUB_X, HUB_Y, HUB_W, HUB_H, CLR_BG);
        _tft.drawRect(HUB_X, HUB_Y, HUB_W, HUB_H, border);
        _hubInv.valid = true;
        _hubInv.color = border;
        _hubInv.line1[0] = _hubInv.line2[0] = '\0';
    }
    if (strcmp(_hubInv.line1, l1) != 0) {
        int16_t ox = HUB_X + (HUB_W - (int16_t)strlen(_hubInv.line1) * CHAR_W) / 2;
        int16_t nx = HUB_X + (HUB_W - (int16_t)strlen(l1)            * CHAR_W) / 2;
        printOver(ox, nx, HUB_Y + 7, 1, CLR_LABEL, CLR_BG, _hubInv.line1, l1);
        strncpy(_hubInv.line1, l1, sizeof(_hubInv.line1) - 1);
        _hubInv.line1[sizeof(_hubInv.line1) - 1] = '\0';
    }
    if (strcmp(_hubInv.line2, l2) != 0) {
        int16_t ox = HUB_X + (HUB_W - (int16_t)strlen(_hubInv.line2) * CHAR_W) / 2;
        int16_t nx = HUB_X + (HUB_W - (int16_t)strlen(l2)            * CHAR_W) / 2;
        printOver(ox, nx, HUB_Y + 23, 1, CLR_TARGET, CLR_BG, _hubInv.line2, l2);
        strncpy(_hubInv.line2, l2, sizeof(_hubInv.line2) - 1);
        _hubInv.line2[sizeof(_hubInv.line2) - 1] = '\0';
    }
}

// Draws one of the 4 L-shaped 2-px arms between a quadrant card and the hub:
//   arm 0: SOLAR (top-left)  <-> HUB
//   arm 1: GRID  (top-right) <-> HUB
//   arm 2: HOME  (bot-left)  <-> HUB
//   arm 3: BATT  (bot-right) <-> HUB
void Display::drawElbowArm(uint8_t arm, bool toHub, uint16_t color) {
    if (arm == 0) {
        // (58, 115) down to (58, 134), right to (74, 134)
        _tft.fillRect(57, 115, 2, 20, color);
        _tft.fillRect(57, 133, 17, 2, color);
        _tft.fillTriangle(74, 134, 68, 130, 68, 138, color);
    } else if (arm == 1) {
        // (182, 115) down to (182, 134), left to (165, 134)
        _tft.fillRect(181, 115, 2, 20, color);
        _tft.fillRect(165, 133, 18, 2, color);
        if (toHub) _tft.fillTriangle(165, 134, 171, 130, 171, 138, color);
        else       _tft.fillTriangle(182, 115, 178, 121, 186, 121, color);
    } else if (arm == 2) {
        // (74, 152) left to (58, 152), down to (58, 170)
        _tft.fillRect(57, 151, 18, 2, color);
        _tft.fillRect(57, 151, 2, 20, color);
        _tft.fillTriangle(58, 170, 54, 164, 62, 164, color);
    } else if (arm == 3) {
        // (165, 152) right to (182, 152), down to (182, 170)
        _tft.fillRect(165, 151, 18, 2, color);
        _tft.fillRect(181, 151, 2, 20, color);
        if (toHub) _tft.fillTriangle(165, 152, 171, 148, 171, 156, color);
        else       _tft.fillTriangle(182, 170, 178, 164, 186, 164, color);
    }
}

void Display::redrawArm(uint8_t arm, bool visible, bool toHub,
                        uint16_t color, ArrowCache& c)
{
    if (c.drawn == visible && c.dir == toHub && c.color == color) return;

    if (c.drawn)
        drawElbowArm(arm, c.dir, CLR_BG);
    if (visible)
        drawElbowArm(arm, toHub, color);

    c.drawn = visible;
    c.dir   = toHub;
    c.color = color;
}

void Display::setTab(Tab t) {
    if (_tab == t) return;
    _tab = t;
    _needFullDraw = true;
}

void Display::invalidateCaches() {
    _cdSolar.valid = _cdGrid.valid = _cdHome.valid = _cdBatt.valid = false;
    _hubInv.valid  = false;
    _arPv.drawn = _arGrid.drawn = _arHome.drawn = _arBatt.drawn = false;
    _btnValid = false;
    _sysValid = false;
    for (auto& l : _sysCache) l[0] = '\0';
    _dotsValid = false;
    _logValid  = false;
}


void Display::update(const InverterData& inv, const BatterySaver& bs,
                     const ModeController& ctrl, const char* sn,
                     bool wifiOk, bool modbusOk, bool mqttOk)
{
    _sn = sn;
    if (!_screenOn) return;   // skip SPI writes while backlight is off

    unsigned long now = millis();
    if (calibrating()) {                 // wizard owns the screen
        if (!_calDrawn) drawCalScreen();
        return;
    }
    if (!_needFullDraw && (now - _lastRedraw < INTERVAL_DISPLAY)) return;
    _lastRedraw = now;

    if (_needFullDraw) {
        invalidateCaches();
        _tft.fillScreen(CLR_BG);
        drawTabBar();
        drawHint();
        _needFullDraw = false;
    }

    drawTopLinkDots(wifiOk, modbusOk, mqttOk);

    if (_tab == TAB_FLOW) updateFlow(inv, bs, ctrl);
    else                  updateSys(inv, bs, ctrl, _sn);
}


void Display::updateFlow(const InverterData& inv, const BatterySaver& bs,
                         const ModeController& ctrl)
{
    char hr[14], v[16], r1[20], r2[20];

    // 1. Top-Left: SOLAR (108 x 82)
    snprintf(hr, sizeof(hr), "%.1fkWh", (double)inv.todayGeneration);
    snprintf(v,  sizeof(v),  "%ld W",   (long)inv.pvPower);
    snprintf(r1, sizeof(r1), "PV1: %5ld W", (long)inv.pv1Power);
    snprintf(r2, sizeof(r2), "PV2: %5ld W", (long)inv.pv2Power);
    redrawCard(CD_LX, CD_TY, CD_W, CD_H, "SOLAR", hr, v, r1, r2, -1, CLR_PV, _cdSolar);

    // 2. Top-Right: GRID (108 x 82)
    int32_t gw = inv.gridPower;
    uint16_t gc = gw >= 0 ? CLR_EXPORT : CLR_IMPORT;
    const char* gDir = gw > 0 ? "EXPORT" : (gw < 0 ? "IMPORT" : "IDLE");
    snprintf(v, sizeof(v), "%ld W", (long)(gw >= 0 ? gw : -gw));
    snprintf(r1, sizeof(r1), "%u/%u/%uV",
             (unsigned)(inv.gridVoltage   + 0.5f),
             (unsigned)(inv.gridVoltageL2 + 0.5f),
             (unsigned)(inv.gridVoltageL3 + 0.5f));
    if (inv.todayImport >= 100.0f || inv.todayExport >= 100.0f) {
        snprintf(r2, sizeof(r2), "In %.0f Out %.0f",
                 (double)inv.todayImport, (double)inv.todayExport);
    } else {
        snprintf(r2, sizeof(r2), "In %.1f Out %.1f",
                 (double)inv.todayImport, (double)inv.todayExport);
    }
    redrawCard(CD_RX, CD_TY, CD_W, CD_H, "GRID", gDir, v, r1, r2, -1, gc, _cdGrid);

    // 3. Bottom-Left: HOME (108 x 82) — no Total line
    unsigned selfPct = 100;
    if (inv.todayConsumption > 0.05f) {
        float netSelf = inv.todayConsumption - inv.todayImport;
        if (netSelf < 0) netSelf = 0;
        selfPct = (unsigned)(netSelf * 100.0f / inv.todayConsumption + 0.5f);
        if (selfPct > 100) selfPct = 100;
    }
    snprintf(hr, sizeof(hr), "Self %u%%", selfPct);
    snprintf(v,  sizeof(v),  "%ld W",     (long)inv.loadPower);
    snprintf(r1, sizeof(r1), "Today %.1f kWh", (double)inv.todayConsumption);
    r2[0] = '\0';
    redrawCard(CD_LX, CD_BY, CD_W, CD_H, "HOME", hr, v, r1, r2, -1, ILI9341_WHITE, _cdHome);

    // 4. Bottom-Right: BATT (108 x 82) + graphical SOC bar
    int32_t bw = inv.batteryPower;
    uint16_t bc = bw >= 0 ? CLR_CHARGE : CLR_DISCHG;
    const char* bDir = bw > 0 ? "CHG" : (bw < 0 ? "DIS" : "IDLE");
    snprintf(v,  sizeof(v),  "%ld W", (long)(bw >= 0 ? bw : -bw));
    snprintf(r1, sizeof(r1), "SOC %u%%   %dC",
             (unsigned)inv.batterySOC, (int)inv.battTemp);
    r2[0] = '\0';
    int8_t soc = inv.batterySOC > 100 ? 100 : (int8_t)inv.batterySOC;
    redrawCard(CD_RX, CD_BY, CD_W, CD_H, "BATT", bDir, v, r1, r2, soc, bc, _cdBatt);

    // 5. Central INV Hub + 4 L-shaped flow arms
    bool passiveOk = !inv.valid || inv.workingMode == WORK_MODE_PASSIVE;
    redrawHub(inv, passiveOk);
    redrawArm(0, inv.pvPower > 0,  true,     CLR_PV,   _arPv);
    redrawArm(1, gw != 0,          gw < 0,   gc,       _arGrid);
    redrawArm(2, inv.loadPower > 0,false,    ILI9341_WHITE, _arHome);
    redrawArm(3, bw != 0,          bw < 0,   bc,       _arBatt);

    // 6. Bottom Action Button
    bool active = bs.isActive();
    char line2[36];
    if (!passiveOk) {
        snprintf(line2, sizeof(line2), "WARN: NOT IN PASSIVE (%u)",
                 (unsigned)inv.workingMode);
    } else if (active && bs.strategy() == BatterySaver::BSAVE_NATIVE) {
        snprintf(line2, sizeof(line2), "NATIVE [0..%ldkW]  PASSIVE",
                 (long)(bs.maxPower() / 1000));
    } else if (active) {
        snprintf(line2, sizeof(line2), "LOOP Target: %ld W",
                 (long)bs.targetPower());
    } else {
        snprintf(line2, sizeof(line2), "Mode: %s  (PASSIVE)",
                 ctrl.currentMode());
    }

    if (!_btnValid || _btnActive != active) {
        uint16_t bg = active ? 0x03E0 : 0x4000;
        _tft.fillRect(BTN_X, BTN_Y, BTN_W, BTN_H, bg);
        _tft.drawRect(BTN_X, BTN_Y, BTN_W, BTN_H, active ? CLR_ACTIVE : CLR_IMPORT);
        _btnValid  = true;
        _btnActive = active;
        _btnLine1[0] = _btnLine2[0] = '\0';   // force text repaint
    }
    uint16_t btnBg = active ? 0x03E0 : 0x4000;
    const char* line1 = active ? "BATT SAVE  ON" : "BATT SAVE OFF";
    uint16_t fg       = active ? ILI9341_WHITE : 0xBDF7;
    if (strcmp(_btnLine1, line1) != 0) {
        int16_t ox = BTN_X + (BTN_W - strlen(_btnLine1) * CHAR_W * 2) / 2;
        int16_t nx = BTN_X + (BTN_W - strlen(line1)     * CHAR_W * 2) / 2;
        printOver(ox, nx, BTN_Y + 6, 2, fg, btnBg, _btnLine1, line1);
        strncpy(_btnLine1, line1, sizeof(_btnLine1) - 1);
        _btnLine1[sizeof(_btnLine1) - 1] = '\0';
    }
    if (strcmp(_btnLine2, line2) != 0) {
        int16_t ox = BTN_X + (BTN_W - strlen(_btnLine2) * CHAR_W) / 2;
        int16_t nx = BTN_X + (BTN_W - strlen(line2)     * CHAR_W) / 2;
        printOver(ox, nx, BTN_Y + 28, 1, CLR_TARGET, btnBg, _btnLine2, line2);
        strncpy(_btnLine2, line2, sizeof(_btnLine2) - 1);
        _btnLine2[sizeof(_btnLine2) - 1] = '\0';
    }
}


void Display::sysPrint(uint8_t idx, int16_t y, const char* text)
{
    if (_sysValid && strcmp(_sysCache[idx], text) == 0) return;
    if (idx < SYS_LINES && _sysCache[idx][0]) {
        _tft.setTextSize(1);
        _tft.setTextColor(CLR_BG, CLR_BG);
        _tft.setCursor(8, y);
        _tft.print(_sysCache[idx]);
    }
    _tft.setTextSize(1);
    _tft.setTextColor(CLR_LABEL, CLR_BG);
    _tft.setCursor(8, y);
    _tft.print(text);
    if (idx < SYS_LINES) {
        strncpy(_sysCache[idx], text, sizeof(_sysCache[0]) - 1);
        _sysCache[idx][sizeof(_sysCache[0]) - 1] = '\0';
    }
    _sysValid = true;
}

void Display::drawLogTail(int16_t y) {
    if (_logValid && _logSerial == appLog.serial()) return;
    _logValid  = true;
    _logSerial = appLog.serial();

    // Rows are capped to the space above the hint line, and wrapping is
    // disabled: print()ing the whole blob let long entries wrap onto extra
    // rows, which is what pushed the log over the footer.
    const int16_t rowH   = LOG_ROW_H;
    const int16_t maxRow = (HINT_Y - 4 - y) / rowH;

    // Walks the log ring in place with a single row buffer: this runs whenever
    // the log changes, and the old 2 kB String plus a substring() per row was
    // the biggest heap churn in the firmware. Now it allocates nothing.
    char row[LOG_COLS];
    unsigned int e = appLog.end();
    unsigned int i = appLog.tailStart(maxRow);

    _tft.fillRect(0, y, 240, maxRow * rowH, CLR_BG);
    _tft.setFont(&Picopixel);          // 3x5: ~2x the log in the same space
    _tft.setTextSize(1);
    _tft.setTextWrap(false);
    _tft.setTextColor(CLR_LABEL, CLR_BG);

    int16_t line = 0;
    unsigned int col = 0;
    for (; i < e && line < maxRow; i++) {
        char c = appLog.at(i);
        if (c != '\n') {
            if (col < sizeof(row) - 1) row[col++] = c;
            continue;
        }
        row[col] = '\0';
        _tft.setCursor(6, y + line * rowH + LOG_BASELINE);  // custom font: baseline
        _tft.print(row);
        line++;
        col = 0;
    }
    if (line < maxRow && col > 0) {     // last entry has no trailing newline
        row[col] = '\0';
        _tft.setCursor(6, y + line * rowH + LOG_BASELINE);
        _tft.print(row);
    }
    _tft.setFont();                    // back to the classic 6x8 font
    _tft.setTextWrap(true);
}

void Display::updateSys(const InverterData& inv, const BatterySaver& bs,
                        const ModeController& ctrl, const char* sn)
{
    char v[64];
    snprintf(v, sizeof(v), "FW: %s   SN: %s", FW_VERSION, sn);
    sysPrint(0, 34, v);

    IPAddress ip = WiFi.localIP();      // no toString(): that allocated a String
    unsigned long up = millis() / 1000;
    snprintf(v, sizeof(v), "IP: %u.%u.%u.%u  Up: %lud %02lu:%02lu",
             ip[0], ip[1], ip[2], ip[3],
             up / 86400, (up / 3600) % 24, (up / 60) % 60);
    sysPrint(1, 46, v);

    snprintf(v, sizeof(v), "RSSI: %ld dBm   Heap: %u/%u kB (%u%%)",
             (long)WiFi.RSSI(),
             (unsigned)(heapStats.freeHeap / 1024),
             (unsigned)(heapStats.maxBlock / 1024), (unsigned)heapStats.frag);
    sysPrint(2, 58, v);

    snprintf(v, sizeof(v), "Temp I/H/B: %d/%d/%dC  Self: %ldW",
             (int)inv.inverterTemp, (int)inv.heatsinkTemp, (int)inv.battTemp,
             (long)inv.inverterSelfPower);
    sysPrint(3, 70, v);

    snprintf(v, sizeof(v), "Mode: %s (%s)  Inv: %u",
             ctrl.currentMode(), bs.strategyName(), (unsigned)inv.workingMode);
    sysPrint(4, 82, v);

    if (!_logValid) {
        _tft.drawFastHLine(0, 96, 240, CLR_OFF);
        _tft.setTextSize(1);
        _tft.setTextColor(CLR_LABEL, CLR_BG);
        _tft.setCursor(8, 101);
        _tft.print("Log:");
    }
    drawLogTail(112);
}


void Display::startCalibration() {
    _calStep  = CAL_A;
    _calDrawn = false;
    _calRetry = false;
    _cal.valid = false;
    // If the wizard was started by a long press, the finger is still down:
    // that press must not be mistaken for the first target tap.
    _calIgnorePress = _touchedPrev;
    _calNotice      = false;
    if (!_screenOn) wake();
    appLog.add("TCH", "calibration started");
}

void Display::drawCalScreen() {
    static const int16_t tx[3] = {TCAL_LO_X, TCAL_HI_X, TCAL_LO_X};
    static const int16_t ty[3] = {TCAL_LO_Y, TCAL_LO_Y, TCAL_HI_Y};
    _tft.fillScreen(CLR_BG);
    drawCentered(120, "TOUCH CALIBRATION", 2, CLR_PV);
    char msg[32];
    snprintf(msg, sizeof(msg), "tap the crosshair  %d of 3", _calStep + 1);
    drawCentered(150, msg, 1, CLR_LABEL);
    if (_calRetry)
        drawCentered(170, "bad samples - starting over", 1, CLR_IMPORT);

    int16_t x = tx[_calStep], y = ty[_calStep];
    _tft.drawCircle(x, y, 10, CLR_LABEL);
    _tft.drawCircle(x, y, 3, CLR_ACTIVE);
    _tft.drawFastHLine(x - 16, y, 33, CLR_ACTIVE);
    _tft.drawFastVLine(x, y - 16, 33, CLR_ACTIVE);
    _calDrawn = true;
}

// Averages the readings taken while the finger is down: a single sample is
// noisy, and the first few counts arrive while contact is still settling.
bool Display::calPollTouch(bool down, unsigned long now) {
    if (_calIgnorePress) {
        if (down) { _lastTouch = now; return false; }
        _calIgnorePress = false;
        _touchedPrev    = false;
        return false;
    }
    if (down) {
        _lastTouch = now;
        if (!_touchedPrev) {
            _touchedPrev = true;
            _pressStart  = now;
            _calSumX = _calSumY = 0;
            _calCount = 0;
            return false;
        }
        if (now - _pressStart >= 60) {          // let the contact settle first
            TS_Point p = _ts.getPoint();
            _calSumX += p.x;
            _calSumY += p.y;
            _calCount++;
        }
        return false;
    }
    if (!_touchedPrev) return false;
    _touchedPrev = false;
    if (_calCount < 2) return false;             // too brief to trust

    _calRaw[_calStep].x = (int16_t)(_calSumX / _calCount);
    _calRaw[_calStep].y = (int16_t)(_calSumY / _calCount);
    snprintf(_touchDbg, sizeof(_touchDbg), "cal %d raw %d,%d n=%d",
             _calStep + 1, (int)_calRaw[_calStep].x, (int)_calRaw[_calStep].y,
             (int)_calCount);
    appLog.add("TCH", _touchDbg);

    if (_calStep < CAL_C) { _calStep++; _calDrawn = false; return false; }

    TouchCal built;
    if (!touchCalBuild(_calRaw[0], _calRaw[1], _calRaw[2], built)) {
        appLog.add("TCH", "calibration rejected: no usable travel");
        _calStep  = CAL_A;
        _calRetry = true;
        _calDrawn = false;
        return false;
    }
    _cal      = built;
    _calStep  = CAL_DONE;
    _needFullDraw = true;
    snprintf(_touchDbg, sizeof(_touchDbg), "cal ok swap=%d x %d..%d y %d..%d",
             (int)_cal.swap, (int)_cal.xLo, (int)_cal.xHi,
             (int)_cal.yLo, (int)_cal.yHi);
    appLog.add("TCH", _touchDbg);
    if (_calSaved) _calSaved(_cal);
    return false;
}

// Gesture-based, so it cannot be broken by panel rotation/wiring:
//   press               -> wake if the backlight is off (consumes the gesture)
//   release before 800ms -> short tap: toggle battery saver (FLOW tab only)
//   held past 800ms      -> switch tab, fires once per press
bool Display::pollTouch() {
    unsigned long now = millis();
    if (now - _lastPollAt < TS_POLL_MS) return false;
    _lastPollAt = now;

    uint32_t lastDown = (uint32_t)_lastDownAt;
    bool down = touchDebouncedDown(_ts.touched(), _touchedPrev, (uint32_t)now,
                                   lastDown);
    _lastDownAt = lastDown;

    if (calibrating()) return calPollTouch(down, now);

    if (down) {
        _lastTouch = now;
        if (!_touchedPrev) {                       // press
            _touchedPrev = true;
            _pressStart  = now;
            _longFired   = false;
            _tapX = _tapY = -1;
            if (!_screenOn) { wake(); _longFired = true; return false; }
            TS_Point p = _ts.getPoint();
            if (_cal.valid) {
                touchCalApply(_cal, p.x, p.y, _tapX, _tapY);
                snprintf(_touchDbg, sizeof(_touchDbg), "raw %d,%d -> %d,%d",
                         (int)p.x, (int)p.y, (int)_tapX, (int)_tapY);
            } else {
                snprintf(_touchDbg, sizeof(_touchDbg), "raw %d,%d (uncal)",
                         (int)p.x, (int)p.y);
            }
            appLog.add("TCH", _touchDbg);
            return false;
        }
        if (!_longFired && now - _pressStart >= TS_LONG_MS) {
            _longFired = true;                     // long press: switch tab
            setTab(_tab == TAB_FLOW ? TAB_SYS : TAB_FLOW);
            appLog.add("TCH", _tab == TAB_SYS ? "long press -> SYS"
                                              : "long press -> FLOW");
        }
        if (!_calNotice && now - _pressStart >= TS_CAL_MS / 2) {
            _calNotice = true;
            drawHint();
        }
        if (now - _pressStart >= TS_CAL_MS) startCalibration();
        return false;
    }

    if (!_touchedPrev) return false;
    _touchedPrev = false;                          // release
    if (_calNotice) { _calNotice = false; drawHint(); }
    bool shortTap = !_longFired && (now - _pressStart) >= TS_MIN_TAP_MS;
    if (!shortTap) return false;

    if (_cal.valid && _tapX >= 0) {                // calibrated: use regions
        if (_tapY < TAB_H) {
            setTab(_tapX < TAB_FLOW_X1 ? TAB_FLOW : TAB_SYS);
            return false;
        }
        bool inBtn = _tab == TAB_FLOW && _tapX >= BTN_X && _tapX < BTN_X + BTN_W
                     && _tapY >= BTN_Y && _tapY < BTN_Y + BTN_H;
        if (inBtn) appLog.add("TCH", "button -> toggle saver");
        return inBtn;
    }
    if (_tab != TAB_FLOW) return false;            // uncalibrated fallback
    appLog.add("TCH", "tap -> toggle saver");
    return true;
}

void Display::wake() {
    _tft.begin();
    _tft.setRotation(2);
    _brightness = BL_FULL;
    analogWrite(PIN_TFT_LED, _brightness);
    _screenOn     = true;
    _needFullDraw = true;
}

void Display::handleDimming() {
    if (_brightness == 0) { _screenOn = false; return; }
    if ((millis() - _lastTouch) > SCREEN_DIM_MS) {
        if (millis() - _lastDimStep < 40) return;   // rate-limit, no blocking
        _lastDimStep = millis();
        _brightness--;
        analogWrite(PIN_TFT_LED, _brightness);
        if (_brightness == 0) _screenOn = false;
    }
}
