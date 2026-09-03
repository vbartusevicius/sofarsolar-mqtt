#include "Display.h"
#include <ESP8266WiFi.h>
#include "Config.h"
#include "inverter/Inverter.h"
#include "control/BatterySaver.h"
#include "control/ModeController.h"
#include "util/AppLog.h"
#include "util/HeapStats.h"
#include "Version.h"

#define CLR_BG       ILI9341_BLACK
#define CLR_PANEL    0x2104            // dark panel fill
#define CLR_LABEL    0xBDF7            // light grey
#define CLR_EXPORT   ILI9341_GREEN
#define CLR_IMPORT   ILI9341_RED
#define CLR_CHARGE   0x04BF            // sky blue
#define CLR_DISCHG   ILI9341_ORANGE
#define CLR_PV       ILI9341_YELLOW
#define CLR_TARGET   ILI9341_CYAN
#define CLR_ACTIVE   ILI9341_GREEN
#define CLR_OFF      0x8410            // dim grey
#define CLR_TAB_BAR  0x10A2            // tab strip background

#define CHAR_W         6   // classic GFX font is 6 px wide at size 1

#define TAB_H         26
#define TAB_FLOW_X0    0
#define TAB_FLOW_X1  120

// Flow-tab node geometry (240x320 portrait)
#define ND_W          120
#define ND_H           54
#define ND_SOLAR_X     60
#define ND_SOLAR_Y     34
#define ND_SIDE_X       8
#define ND_SIDE_Y      122
#define ND_SIDE_W      104
#define ND_GRID_X       8
#define ND_BATT_X     128
#define ND_HOME_X      60
#define ND_HOME_Y     208

#define BTN_Y         268
#define BTN_H          44
#define BTN_X           8
#define BTN_W         224

// XPT2046 raw ranges (after rotation); trim these if taps are offset
#define TS_RAW_X_MIN  250
#define TS_RAW_X_MAX 3800
#define TS_RAW_Y_MIN  300
#define TS_RAW_Y_MAX 3850

Display::Display()
    : _tft(PIN_TFT_CS, PIN_TFT_DC),
      _ts(PIN_TOUCH_CS, PIN_TOUCH_IRQ)
{}

void Display::begin() {
    _tft.begin();
    _tft.setRotation(2);             // portrait, USB at top
    _ts.begin();
    _ts.setRotation(1);
    analogWrite(PIN_TFT_LED, 32);
    _brightness = 32;
    _screenOn   = true;
    _lastTouch  = millis();
    _needFullDraw = true;
}

void Display::showSplash(const char* line1, const char* line2) {
    _tft.fillScreen(CLR_BG);
    drawCentered(120, String(line1), 2, ILI9341_WHITE);
    drawCentered(150, String(line2), 2, CLR_PV);
}

// ── Shared drawing helpers ─────────────────────────────────────
void Display::drawCentered(int16_t y, const String& text, uint8_t size,
                           uint16_t fg)
{
    int16_t w = text.length() * CHAR_W * size;
    _tft.setTextSize(size);
    _tft.setTextColor(fg, CLR_BG);
    _tft.setCursor((240 - w) / 2, y);
    _tft.print(text);
}

void Display::drawTabBar() {
    _tft.fillRect(0, 0, 240, TAB_H, CLR_TAB_BAR);
    const uint16_t activeBg = 0x0010;                 // dark blue
    for (int i = 0; i < 2; i++) {
        int16_t x = i * 120;
        bool active = ((int)_tab == i);
        if (active) _tft.fillRect(x, 0, 120, TAB_H, activeBg);
        const char* label = i == 0 ? "FLOW" : "SYS";
        _tft.setTextSize(2);
        _tft.setTextColor(active ? ILI9341_WHITE : CLR_OFF,
                          active ? activeBg : CLR_TAB_BAR);
        int16_t tw = strlen(label) * CHAR_W * 2;
        _tft.setCursor(x + (120 - tw) / 2, 6);
        _tft.print(label);
        if (i > 0) _tft.drawFastVLine(x, 2, TAB_H - 4, CLR_OFF);
    }
    _tft.drawFastHLine(0, TAB_H, 240, CLR_OFF);
}

void Display::drawNode(int16_t x, int16_t y, int16_t w, int16_t h,
                       const char* name, const char* value, const char* sub,
                       uint16_t color)
{
    _tft.fillRect(x, y, w, h, CLR_PANEL);
    _tft.drawRect(x, y, w, h, color);
    _tft.setTextSize(1);
    _tft.setTextColor(CLR_LABEL, CLR_PANEL);
    _tft.setCursor(x + 5, y + 4);
    _tft.print(name);

    int16_t vw = strlen(value) * CHAR_W * 2;
    _tft.setTextSize(2);
    _tft.setTextColor(color, CLR_PANEL);
    _tft.setCursor(x + (w - vw) / 2, y + 15);
    _tft.print(value);

    if (sub && sub[0]) {
        int16_t sw = strlen(sub) * CHAR_W;
        _tft.setTextSize(1);
        _tft.setTextColor(CLR_LABEL, CLR_PANEL);
        _tft.setCursor(x + (w - sw) / 2, y + h - 13);
        _tft.print(sub);
    }
}

void Display::drawArrow(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                        uint16_t color, const char* label)
{
    _tft.drawLine(x0, y0, x1, y1, color);
    float dx = x1 - x0, dy = y1 - y0;
    float len = sqrtf(dx * dx + dy * dy);
    if (len < 1) len = 1;
    float ux = dx / len, uy = dy / len;
    // Arrowhead
    int16_t hx1 = x1 - (int16_t)(12 * ux + 6 * uy);
    int16_t hy1 = y1 - (int16_t)(12 * uy - 6 * ux);
    int16_t hx2 = x1 - (int16_t)(12 * ux - 6 * uy);
    int16_t hy2 = y1 - (int16_t)(12 * uy + 6 * ux);
    _tft.drawLine(x1, y1, hx1, hy1, color);
    _tft.drawLine(x1, y1, hx2, hy2, color);
    // Label beside the midpoint (perpendicular offset)
    int16_t mx = (x0 + x1) / 2 - (int16_t)(12 * uy);
    int16_t my = (y0 + y1) / 2 + (int16_t)(12 * ux) - 4;
    _tft.setTextSize(1);
    _tft.setTextColor(color, CLR_BG);
    _tft.setCursor(mx, my);
    _tft.print(label);
}

void Display::setTab(Tab t) {
    if (_tab == t) return;
    _tab = t;
    _needFullDraw = true;
}

// ── Top-level update ───────────────────────────────────────────
void Display::update(const InverterData& inv, const BatterySaver& bs,
                     const ModeController& ctrl, const char* sn,
                     bool wifiOk, bool modbusOk, bool mqttOk)
{
    _sn = sn;
    if (!_screenOn) return;   // skip SPI writes while backlight is off

    unsigned long now = millis();
    if (!_needFullDraw && (now - _lastRedraw < INTERVAL_DISPLAY)) return;
    _lastRedraw = now;

    if (_needFullDraw) {
        _tft.fillScreen(CLR_BG);
        drawTabBar();
        _needFullDraw = false;
    }

    if (_tab == TAB_FLOW) updateFlow(inv, bs, ctrl);
    else                  updateSys(inv, _sn, wifiOk, modbusOk, mqttOk);
}

// ── FLOW tab ───────────────────────────────────────────────────
static void fmtW(char* buf, size_t n, int32_t w) {
    snprintf(buf, n, "%ld W", (long)w);
}

void Display::drawSaverButton(const BatterySaver& bs, const ModeController& ctrl) {
    bool active = bs.isActive();
    uint16_t bg  = active ? 0x03E0 : 0x4000;   // dark green / dark red
    uint16_t fg  = active ? ILI9341_WHITE : 0xBDF7;
    _tft.fillRect(BTN_X, BTN_Y, BTN_W, BTN_H, bg);
    _tft.drawRect(BTN_X, BTN_Y, BTN_W, BTN_H, active ? CLR_ACTIVE : CLR_IMPORT);

    _tft.setTextSize(2);
    _tft.setTextColor(fg, bg);
    const char* label = active ? "BATT SAVE  ON" : "BATT SAVE OFF";
    int16_t tw = strlen(label) * CHAR_W * 2;
    _tft.setCursor(BTN_X + (BTN_W - tw) / 2, BTN_Y + 7);
    _tft.print(label);

    char sub[32];
    if (active) snprintf(sub, sizeof(sub), "Target: %ld W", (long)bs.targetPower());
    else        snprintf(sub, sizeof(sub), "Mode: %s", ctrl.currentMode());
    _tft.setTextSize(1);
    _tft.setTextColor(CLR_TARGET, bg);
    int16_t sw = strlen(sub) * CHAR_W;
    _tft.setCursor(BTN_X + (BTN_W - sw) / 2, BTN_Y + 29);
    _tft.print(sub);
}

void Display::updateFlow(const InverterData& inv, const BatterySaver& bs,
                         const ModeController& ctrl)
{
    char v[24], s[24];

    // Solar
    fmtW(v, sizeof(v), inv.pvPower);
    snprintf(s, sizeof(s), "today %.1f kWh", (double)inv.todayGeneration);
    drawNode(ND_SOLAR_X, ND_SOLAR_Y, ND_W, ND_H, " Solar", v, s, CLR_PV);

    // Grid
    uint16_t gc = inv.gridPower >= 0 ? CLR_EXPORT : CLR_IMPORT;
    char gw[24];
    if (inv.gridPower >= 0) snprintf(gw, sizeof(gw), "%ld W", (long)inv.gridPower);
    else                    snprintf(gw, sizeof(gw), "%ld W", (long)-inv.gridPower);
    snprintf(s, sizeof(s), "exp %.1f imp %.1f",
             (double)inv.todayExport, (double)inv.todayImport);
    drawNode(ND_GRID_X, ND_SIDE_Y, ND_SIDE_W, ND_H, " Grid", gw, s, gc);

    // Battery (1+2 combined)
    uint16_t bc = inv.batteryPower >= 0 ? CLR_CHARGE : CLR_DISCHG;
    fmtW(v, sizeof(v), inv.batteryPower >= 0 ? inv.batteryPower : -inv.batteryPower);
    snprintf(s, sizeof(s), "SOC %u%%  +%.1f kWh",
             (unsigned)inv.batterySOC, (double)inv.todayCharged);
    drawNode(ND_BATT_X, ND_SIDE_Y, ND_SIDE_W, ND_H, " Battery", v, s, bc);

    // Home
    fmtW(v, sizeof(v), inv.loadPower);
    snprintf(s, sizeof(s), "use %.1f kWh", (double)inv.todayConsumption);
    drawNode(ND_HOME_X, ND_HOME_Y, ND_W, ND_H, " Home", v, s, ILI9341_WHITE);

    // Arrows
    char lb[16];
    snprintf(lb, sizeof(lb), "%ldW", (long)inv.pvPower);
    if (inv.pvPower > 0)
        drawArrow(ND_SOLAR_X + ND_W / 2, ND_SOLAR_Y + ND_H,
                  ND_HOME_X + ND_W / 2, ND_HOME_Y, CLR_PV, lb);

    snprintf(lb, sizeof(lb), "%ldW", (long)(inv.gridPower >= 0 ? inv.gridPower : -inv.gridPower));
    if (inv.gridPower >= 0)
        drawArrow(ND_HOME_X + 30, ND_HOME_Y, ND_GRID_X + 52, ND_SIDE_Y + ND_H, CLR_EXPORT, lb);
    else
        drawArrow(ND_GRID_X + 52, ND_SIDE_Y + ND_H, ND_HOME_X + 30, ND_HOME_Y, CLR_IMPORT, lb);

    snprintf(lb, sizeof(lb), "%ldW", (long)(inv.batteryPower >= 0 ? inv.batteryPower : -inv.batteryPower));
    if (inv.batteryPower >= 0)
        drawArrow(ND_HOME_X + ND_W - 30, ND_HOME_Y, ND_BATT_X + 52, ND_SIDE_Y + ND_H, CLR_CHARGE, lb);
    else
        drawArrow(ND_BATT_X + 52, ND_SIDE_Y + ND_H, ND_HOME_X + ND_W - 30, ND_HOME_Y, CLR_DISCHG, lb);

    // Saver button with current mode
    drawSaverButton(bs, ctrl);
}

// ── SYS tab ────────────────────────────────────────────────────
void Display::sysLine(int16_t& y, const char* label, const char* value,
                      uint16_t color, bool twoCol, const char* label2,
                      const char* value2, uint16_t color2)
{
    _tft.fillRect(0, y, 240, 10, CLR_BG);
    _tft.setTextSize(1);
    _tft.setTextColor(CLR_LABEL, CLR_BG);
    _tft.setCursor(8, y);
    _tft.print(label);
    _tft.setTextColor(color, CLR_BG);
    _tft.print(value);
    if (twoCol && label2) {
        _tft.setTextColor(CLR_LABEL, CLR_BG);
        _tft.setCursor(124, y);
        _tft.print(label2);
        _tft.setTextColor(color2, CLR_BG);
        _tft.print(value2);
    }
    y += 12;
}

void Display::drawStatusDots(bool wifiOk, bool modbusOk, bool mqttOk) {
    int16_t y = 108;
    _tft.setTextSize(1);
    _tft.setTextColor(CLR_LABEL, CLR_BG);
    _tft.fillRect(0, y, 240, 16, CLR_BG);
    _tft.setCursor(8, y + 4);
    _tft.print("Link:");
    _tft.fillCircle(56, y + 8, 5, wifiOk ? CLR_ACTIVE : CLR_IMPORT);
    _tft.setCursor(66, y + 4);  _tft.print("WiFi");
    _tft.fillCircle(112, y + 8, 5, modbusOk ? CLR_ACTIVE : CLR_IMPORT);
    _tft.setCursor(122, y + 4); _tft.print("RS485");
    _tft.fillCircle(178, y + 8, 5, mqttOk ? CLR_ACTIVE : CLR_IMPORT);
    _tft.setCursor(188, y + 4); _tft.print("MQTT");
}

void Display::drawLogTail(int16_t y) {
    String text = appLog.text();
    int end = text.length() - 1;
    while (end >= 0 && text[end] == '\n') end--;   // trim trailing newlines
    int start = 0, lines = 0;
    for (int i = end; i >= 0; i--) {
        if (text[i] == '\n') {
            lines++;
            if (lines >= 7) { start = i + 1; break; }
        }
    }
    _tft.fillRect(0, y, 240, 316 - y, CLR_BG);
    _tft.setTextSize(1);
    _tft.setTextColor(CLR_OFF, CLR_BG);
    _tft.setCursor(8, y);
    if (start < end) _tft.print(text.substring(start, end + 1));
}

void Display::updateSys(const InverterData& inv, const char* sn,
                        bool wifiOk, bool modbusOk, bool mqttOk)
{
    char v[48], v2[24];
    int16_t y = 36;

    String ip = WiFi.localIP().toString();
    sysLine(y, "FW: ", FW_VERSION, CLR_PV, true, "IP: ", ip.c_str(), CLR_LABEL);

    unsigned long up = millis() / 1000;
    snprintf(v, sizeof(v), "%lud %02lu:%02lu", up / 86400, (up / 3600) % 24, (up / 60) % 60);
    snprintf(v2, sizeof(v2), "%ld dBm", (long)WiFi.RSSI());
    sysLine(y, "Up: ", v, CLR_LABEL, true, "RSSI: ", v2, CLR_LABEL);

    snprintf(v, sizeof(v), "%u/%u kB",
             (unsigned)(heapStats.freeHeap / 1024), (unsigned)(heapStats.maxBlock / 1024));
    snprintf(v2, sizeof(v2), "%u%%", (unsigned)heapStats.frag);
    sysLine(y, "Heap free/blk: ", v, CLR_LABEL, true, "Frag: ", v2,
            heapStats.frag > 40 ? CLR_IMPORT : CLR_ACTIVE);

    sysLine(y, "Inverter SN: ", sn, CLR_LABEL, false, nullptr, nullptr, 0);

    drawStatusDots(wifiOk, modbusOk, mqttOk);

    _tft.drawFastHLine(0, 130, 240, CLR_OFF);
    _tft.fillRect(0, 132, 240, 14, CLR_BG);
    _tft.setTextColor(CLR_LABEL, CLR_BG);
    _tft.setCursor(8, 136);
    _tft.print("Log:");
    drawLogTail(148);
}

// ── Touch / backlight ──────────────────────────────────────────
bool Display::pollTouch() {
    if (!_ts.tirqTouched()) { _touchedPrev = false; return false; }
    if (!_ts.touched())     { _touchedPrev = false; return false; }
    if (_touchedPrev)       return false;   // de-bounce: one event per press

    _touchedPrev = true;
    _lastTouch   = millis();

    if (!_screenOn) { wake(); return false; }   // first tap wakes screen

    TS_Point p = _ts.getPoint();
    int16_t sx = map(p.x, TS_RAW_X_MIN, TS_RAW_X_MAX, 0, 240);
    int16_t sy = map(p.y, TS_RAW_Y_MIN, TS_RAW_Y_MAX, 0, 320);

    if (sy >= 0 && sy < TAB_H) {           // tab bar
        setTab(sx < TAB_FLOW_X1 ? TAB_FLOW : TAB_SYS);
        return false;
    }
    if (_tab == TAB_FLOW && sy >= BTN_Y)   // saver toggle button
        return true;
    return false;
}

void Display::wake() {
    _tft.begin();                // re-init SPI + ILI9341 command sequence
    _tft.setRotation(2);
    _brightness = 32;
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
