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

#define CHAR_W         6   // classic GFX font is 6 px wide at size 1

#define TAB_H         32
#define TAB_FLOW_X1  120

// Flow-tab node geometry (240x320 portrait)
#define ND_W          120
#define ND_H           54
#define ND_SOLAR_X     60
#define ND_SOLAR_Y     34
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

// XPT2046 mapping knobs.  If taps land wrong, /log shows "raw -> scr" for
// every tap; flip these four switches / ranges to match the panel.
#define TS_SWAP_AXES  1   // raw X travels with screen Y
#define TS_INV_X      0   // invert screen X direction
#define TS_INV_Y      0   // invert screen Y direction
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

void Display::drawTabBar() {
    const uint16_t barBg    = 0x0841;   // dim grey-blue
    const uint16_t activeBg = 0x0010;   // dark blue
    _tft.fillRect(0, 0, 240, TAB_H, barBg);
    for (int i = 0; i < 2; i++) {
        int16_t x = i * 120;
        bool active = ((int)_tab == i);
        if (active) _tft.fillRect(x, 0, 120, TAB_H, activeBg);
        const char* label = i == 0 ? "FLOW" : "SYS";
        _tft.setTextSize(2);
        _tft.setTextColor(active ? ILI9341_WHITE : 0xC618,
                          active ? activeBg : barBg);
        int16_t tw = strlen(label) * CHAR_W * 2;
        _tft.setCursor(x + (120 - tw) / 2, 7);
        _tft.print(label);
        if (active)
            _tft.fillRect(x, TAB_H - 3, 120, 3, CLR_PV);   // accent underline
        if (i > 0) _tft.drawFastVLine(x, 4, TAB_H - 8, CLR_OFF);
    }
    _tft.drawFastHLine(0, TAB_H, 240, CLR_OFF);
}

void Display::drawNodeFrame(int16_t x, int16_t y, int16_t w, int16_t h,
                            const char* name)
{
    _tft.fillRect(x, y, w, h, CLR_PANEL);
    _tft.setTextSize(1);
    _tft.setTextColor(CLR_LABEL, CLR_PANEL);
    _tft.setCursor(x + 5, y + 4);
    _tft.print(name);
}

void Display::redrawNode(int16_t x, int16_t y, int16_t w, int16_t h,
                         const char* name, const char* value, const char* sub,
                         uint16_t color, NodeCache& c)
{
    // Border colour change (export→import etc.): repaint the whole node —
    // the old text must be erased too, not just the frame.
    if (!c.valid || c.color != color) {
        _tft.fillRect(x, y, w, h, CLR_BG);
        drawNodeFrame(x, y, w, h, name);
        _tft.drawRect(x, y, w, h, color);
        c.valid = true;
        c.color = color;
        c.value[0] = c.sub[0] = '\0';                // force text repaint
    }
    if (strcmp(c.value, value) != 0) {
        int16_t ox = x + (w - strlen(c.value) * CHAR_W * 2) / 2;
        int16_t nx = x + (w - strlen(value)    * CHAR_W * 2) / 2;
        printOver(ox, nx, y + 15, 2, color, CLR_PANEL, c.value, value);
        strncpy(c.value, value, sizeof(c.value) - 1);
        c.value[sizeof(c.value) - 1] = '\0';
    }
    if (strcmp(c.sub, sub) != 0) {
        int16_t ox = x + (w - strlen(c.sub) * CHAR_W) / 2;
        int16_t nx = x + (w - strlen(sub)    * CHAR_W) / 2;
        printOver(ox, nx, y + h - 13, 1, CLR_LABEL, CLR_PANEL, c.sub, sub);
        strncpy(c.sub, sub, sizeof(c.sub) - 1);
        c.sub[sizeof(c.sub) - 1] = '\0';
    }
}

void Display::drawArrowLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                            uint16_t color)
{
    _tft.drawLine(x0, y0, x1, y1, color);
    float dx = x1 - x0, dy = y1 - y0;
    float len = sqrtf(dx * dx + dy * dy);
    if (len < 1) len = 1;
    float ux = dx / len, uy = dy / len;
    int16_t hx1 = x1 - (int16_t)(12 * ux + 6 * uy);
    int16_t hy1 = y1 - (int16_t)(12 * uy - 6 * ux);
    int16_t hx2 = x1 - (int16_t)(12 * ux - 6 * uy);
    int16_t hy2 = y1 - (int16_t)(12 * uy + 6 * ux);
    _tft.drawLine(x1, y1, hx1, hy1, color);
    _tft.drawLine(x1, y1, hx2, hy2, color);
}

void Display::redrawArrow(const ArrowDef& d, bool forward, uint16_t color,
                          const char* label, ArrowCache& c)
{
    if (c.drawn && c.dir == forward && strcmp(c.label, label) == 0) return;

    if (c.drawn) {   // erase old: same pixels in background colour
        drawArrowLine(c.dir ? d.fx : d.tx, c.dir ? d.fy : d.ty,
                      c.dir ? d.tx : d.fx, c.dir ? d.ty : d.fy, CLR_BG);
        _tft.setTextSize(1);
        _tft.setTextColor(CLR_BG, CLR_BG);
        _tft.setCursor(d.lx, d.ly);
        _tft.print(c.label);
    }

    if (label[0] != '\0') {
        drawArrowLine(forward ? d.fx : d.tx, forward ? d.fy : d.ty,
                      forward ? d.tx : d.fx, forward ? d.ty : d.fy, color);
        _tft.setTextSize(1);
        _tft.setTextColor(color, CLR_BG);
        _tft.setCursor(d.lx, d.ly);
        _tft.print(label);
        c.dir   = forward;
        strncpy(c.label, label, sizeof(c.label) - 1);
        c.label[sizeof(c.label) - 1] = '\0';
        c.drawn = true;
    } else {
        c.drawn = false;
    }
}

void Display::setTab(Tab t) {
    if (_tab == t) return;
    _tab = t;
    _needFullDraw = true;
}

void Display::invalidateCaches() {
    _ndSolar.valid = _ndGrid.valid = _ndBatt.valid = _ndHome.valid = false;
    _arPv.drawn = _arGrid.drawn = _arBatt.drawn = false;
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
    if (!_needFullDraw && (now - _lastRedraw < INTERVAL_DISPLAY)) return;
    _lastRedraw = now;

    if (_needFullDraw) {
        invalidateCaches();
        _tft.fillScreen(CLR_BG);
        drawTabBar();
        _needFullDraw = false;
    }

    if (_tab == TAB_FLOW) updateFlow(inv, bs, ctrl);
    else                  updateSys(inv, _sn, wifiOk, modbusOk, mqttOk);
}


void Display::updateFlow(const InverterData& inv, const BatterySaver& bs,
                         const ModeController& ctrl)
{
    char v[24], s[32];

    snprintf(v, sizeof(v), "%ld W", (long)inv.pvPower);
    snprintf(s, sizeof(s), "today %.1f kWh", (double)inv.todayGeneration);
    redrawNode(ND_SOLAR_X, ND_SOLAR_Y, ND_W, ND_H, " Solar", v, s, CLR_PV, _ndSolar);

    uint16_t gc = inv.gridPower >= 0 ? CLR_EXPORT : CLR_IMPORT;
    snprintf(v, sizeof(v), "%ld W", (long)(inv.gridPower >= 0 ? inv.gridPower : -inv.gridPower));
    snprintf(s, sizeof(s), "exp %.1f imp %.1f",
             (double)inv.todayExport, (double)inv.todayImport);
    redrawNode(ND_GRID_X, ND_SIDE_Y, ND_SIDE_W, ND_H, " Grid", v, s, gc, _ndGrid);

    uint16_t bc = inv.batteryPower >= 0 ? CLR_CHARGE : CLR_DISCHG;
    snprintf(v, sizeof(v), "%ld W", (long)(inv.batteryPower >= 0 ? inv.batteryPower : -inv.batteryPower));
    snprintf(s, sizeof(s), "SOC %u%% +%.1f kWh",
             (unsigned)inv.batterySOC, (double)inv.todayCharged);
    redrawNode(ND_BATT_X, ND_SIDE_Y, ND_SIDE_W, ND_H, " Battery", v, s, bc, _ndBatt);

    snprintf(v, sizeof(v), "%ld W", (long)inv.loadPower);
    snprintf(s, sizeof(s), "use %.1f kWh", (double)inv.todayConsumption);
    redrawNode(ND_HOME_X, ND_HOME_Y, ND_W, ND_H, " Home", v, s, ILI9341_WHITE, _ndHome);

    // Arrows (endpoints given in "forward" direction; label pos pre-laid-out)
    static const ArrowDef AR_PV   = { 120,  88, 120, 208,  96, 140 };
    static const ArrowDef AR_GRID = {  90, 208,  60, 176,  82, 183 };
    static const ArrowDef AR_BATT = { 150, 208, 180, 176, 173, 201 };

    char lb[12];
    if (inv.pvPower > 0) snprintf(lb, sizeof(lb), "%ldW", (long)inv.pvPower);
    else                 lb[0] = '\0';
    redrawArrow(AR_PV, true, CLR_PV, lb, _arPv);

    int32_t gw = inv.gridPower;
    snprintf(lb, sizeof(lb), "%ldW", (long)(gw >= 0 ? gw : -gw));
    redrawArrow(AR_GRID, gw >= 0, gw >= 0 ? CLR_EXPORT : CLR_IMPORT, lb, _arGrid);

    int32_t bw = inv.batteryPower;
    snprintf(lb, sizeof(lb), "%ldW", (long)(bw >= 0 ? bw : -bw));
    redrawArrow(AR_BATT, bw >= 0, bw >= 0 ? CLR_CHARGE : CLR_DISCHG, lb, _arBatt);

    bool active = bs.isActive();
    char line2[36];
    if (active) snprintf(line2, sizeof(line2), "Target: %ld W", (long)bs.targetPower());
    else        snprintf(line2, sizeof(line2), "Mode: %s", ctrl.currentMode());

    if (!_btnValid || _btnActive != active) {
        uint16_t bg = active ? 0x03E0 : 0x4000;
        _tft.fillRect(BTN_X, BTN_Y, BTN_W, BTN_H, bg);
        _tft.drawRect(BTN_X, BTN_Y, BTN_W, BTN_H, active ? CLR_ACTIVE : CLR_IMPORT);
        _btnValid = true;
        _btnLine1[0] = _btnLine2[0] = '\0';   // force text repaint
    }
    uint16_t btnBg = active ? 0x03E0 : 0x4000;
    const char* line1 = active ? "BATT SAVE  ON" : "BATT SAVE OFF";
    uint16_t fg       = active ? ILI9341_WHITE : 0xBDF7;
    if (strcmp(_btnLine1, line1) != 0) {
        int16_t ox = BTN_X + (BTN_W - strlen(_btnLine1) * CHAR_W * 2) / 2;
        int16_t nx = BTN_X + (BTN_W - strlen(line1)     * CHAR_W * 2) / 2;
        printOver(ox, nx, BTN_Y + 7, 2, fg, btnBg, _btnLine1, line1);
        strcpy(_btnLine1, line1);
    }
    if (strcmp(_btnLine2, line2) != 0) {
        int16_t ox = BTN_X + (BTN_W - strlen(_btnLine2) * CHAR_W) / 2;
        int16_t nx = BTN_X + (BTN_W - strlen(line2)     * CHAR_W) / 2;
        printOver(ox, nx, BTN_Y + 29, 1, CLR_TARGET, btnBg, _btnLine2, line2);
        strcpy(_btnLine2, line2);
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

void Display::drawStatusDots(bool wifiOk, bool modbusOk, bool mqttOk) {
    uint8_t state = (wifiOk ? 1 : 0) | (modbusOk ? 2 : 0) | (mqttOk ? 4 : 0);
    if (_dotsValid && _dotsCache == state) return;
    _dotsValid = true;
    _dotsCache = state;

    int16_t y = 120;
    _tft.setTextSize(1);
    _tft.setTextColor(CLR_LABEL, CLR_BG);
    _tft.setCursor(8, y + 4);
    _tft.print("Link:");
    _tft.fillCircle(56,  y + 8, 5, wifiOk   ? CLR_ACTIVE : CLR_IMPORT);
    _tft.setCursor(66, y + 4);   _tft.print("WiFi");
    _tft.fillCircle(112, y + 8, 5, modbusOk ? CLR_ACTIVE : CLR_IMPORT);
    _tft.setCursor(122, y + 4);  _tft.print("RS485");
    _tft.fillCircle(178, y + 8, 5, mqttOk   ? CLR_ACTIVE : CLR_IMPORT);
    _tft.setCursor(188, y + 4);  _tft.print("MQTT");
}

void Display::drawLogTail(int16_t y) {
    if (_logValid && _logSerial == appLog.serial()) return;
    _logValid  = true;
    _logSerial = appLog.serial();

    String text = appLog.text();
    int end = text.length() - 1;
    while (end >= 0 && text[end] == '\n') end--;   // trim trailing newlines
    int start = 0, lines = 0;
    for (int i = end; i >= 0; i--) {
        if (text[i] == '\n') {
            lines++;
            if (lines >= LOG_LINES_MAX) { start = i + 1; break; }
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
    char v[64];
    String ip = WiFi.localIP().toString();
    snprintf(v, sizeof(v), "FW: %s   IP: %s", FW_VERSION, ip.c_str());
    sysPrint(0, 40, v);

    unsigned long up = millis() / 1000;
    snprintf(v, sizeof(v), "Up: %lud %02lu:%02lu   RSSI: %ld dBm",
             up / 86400, (up / 3600) % 24, (up / 60) % 60, (long)WiFi.RSSI());
    sysPrint(1, 52, v);

    snprintf(v, sizeof(v), "Heap free/blk: %u/%u kB   frag %u%%",
             (unsigned)(heapStats.freeHeap / 1024),
             (unsigned)(heapStats.maxBlock / 1024), (unsigned)heapStats.frag);
    sysPrint(2, 64, v);

    snprintf(v, sizeof(v), "Inverter SN: %s", sn);
    sysPrint(3, 76, v);

    snprintf(v, sizeof(v), "Touch: %s", _touchDbg[0] ? _touchDbg : "-");
    sysPrint(4, 88, v);

    drawStatusDots(wifiOk, modbusOk, mqttOk);

    if (!_logValid) {
        _tft.drawFastHLine(0, 140, 240, CLR_OFF);
        _tft.setTextSize(1);
        _tft.setTextColor(CLR_LABEL, CLR_BG);
        _tft.setCursor(8, 146);
        _tft.print("Log:");
    }
    drawLogTail(158);
}


bool Display::pollTouch() {
    if (!_ts.tirqTouched()) { _touchedPrev = false; return false; }
    if (!_ts.touched())     { _touchedPrev = false; return false; }
    if (_touchedPrev)       return false;   // de-bounce: one event per press

    _touchedPrev = true;
    _lastTouch   = millis();

    if (!_screenOn) { wake(); return false; }

    TS_Point p = _ts.getPoint();
    int16_t rx = TS_SWAP_AXES ? p.y : p.x;
    int16_t ry = TS_SWAP_AXES ? p.x : p.y;
    int16_t sx = map(rx, TS_RAW_X_MIN, TS_RAW_X_MAX, TS_INV_X ? 240 : 0, TS_INV_X ? 0 : 240);
    int16_t sy = map(ry, TS_RAW_Y_MIN, TS_RAW_Y_MAX, TS_INV_Y ? 320 : 0, TS_INV_Y ? 0 : 320);

    snprintf(_touchDbg, sizeof(_touchDbg), "raw %d,%d -> scr %d,%d",
             (int)p.x, (int)p.y, (int)sx, (int)sy);
    appLog.add("TCH", _touchDbg);

    if (sy >= 0 && sy < TAB_H) {           // tab bar
        setTab(sx < TAB_FLOW_X1 ? TAB_FLOW : TAB_SYS);
        return false;
    }
    if (_tab == TAB_FLOW && sy >= BTN_Y)   // saver toggle button
        return true;
    return false;
}

void Display::wake() {
    _tft.begin();
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
