#include "Display.h"
#include <Fonts/Picopixel.h>
#include "hw/Pins.h"
#include "Tuning.h"
#include "inverter/InverterData.h"
#include "util/AppLog.h"

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

#define CHAR_W       FM_CHAR_W   // classic GFX font is 6 px wide at size 1

#define TAB_H         28
#define TAB_FLOW_X1   86
#define TAB_SYS_X1   172

// 2x2 Quadrant Card geometry + Central INV Hub (240x320 portrait)
#define CD_W         FM_CARD_W
#define CD_H          82
#define CD_LX          4
#define CD_RX        128
#define CD_TY         32
#define CD_BY        172

#define HUB_X         76
#define HUB_Y        123
#define HUB_W        FM_HUB_W
#define HUB_H         40

#define BTN_Y        260
#define BTN_H         42
#define HINT_Y       311
#define SYS_Y0        33
#define SYS_DY        10
#define LOG_ROW_H      7   // Picopixel yAdvance
#define LOG_BASELINE   5   // glyph height above the baseline
#define LOG_COLS      60   // 232 px / ~4 px per Picopixel char, + NUL
#define BTN_X          4
#define BTN_W        FM_BTN_W

Display::Display()
    : _tft(PIN_TFT_CS, PIN_TFT_DC),
      _touch(PIN_TOUCH_CS, PIN_TOUCH_IRQ)
{}

void Display::begin() {
    _tft.begin();
    _tft.setRotation(2);             // portrait, USB at top
    _touch.begin();
    analogWrite(PIN_TFT_LED, BL_FULL);
    _brightness = BL_FULL;
    _screenOn   = true;
    _needFullDraw = true;
}

void Display::showSplash(const char* line1, const char* line2) {
    _tft.fillScreen(CLR_BG);
    drawCentered(120, line1, 2, ILI9341_WHITE);
    drawCentered(150, line2, 2, CLR_PV);
}

void Display::drawCentered(int16_t y, const char* text, uint8_t size,
                           uint16_t fg)
{
    _tft.setTextSize(size);
    _tft.setTextColor(fg, CLR_BG);
    _tft.setCursor(alignX(0, 240, AL_CENTER, text, size), y);
    _tft.print(text);
}

uint16_t Display::toneColor(Tone t) {
    switch (t) {
    case Tone::Pv:        return CLR_PV;
    case Tone::Export:    return CLR_EXPORT;
    case Tone::Import:    return CLR_IMPORT;
    case Tone::Charge:    return CLR_CHARGE;
    case Tone::Discharge: return CLR_DISCHG;
    case Tone::Neutral:   return ILI9341_WHITE;
    }
    return ILI9341_WHITE;
}

int16_t Display::alignX(int16_t x, int16_t w, Align a, const char* s,
                        uint8_t size)
{
    int16_t tw = (int16_t)strlen(s) * CHAR_W * size;
    switch (a) {
    case AL_LEFT:   return x;
    case AL_CENTER: return x + (w - tw) / 2;
    case AL_RIGHT:  return x + w - tw;
    }
    return x;
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
    _tft.setCursor(alignX(0, 240, AL_CENTER, text, 1), HINT_Y);
    _tft.print(text);
}

void Display::drawHint() {
    if (_touch.calNotice()) { drawHintLine("keep holding for calibration..."); return; }
    if (_touch.cal().valid) drawHintLine("tap tabs/button   hold: switch tab");
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
        _tft.setCursor(alignX(x, TAB_FLOW_X1, AL_CENTER, label, 2), 5);
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

void Display::redrawCard(int16_t x, int16_t y, const CardText& t, CardCache& c)
{
    const int16_t w = CD_W, h = CD_H;
    uint16_t color = toneColor(t.tone);
    if (!c.valid || c.color != color) {
        _tft.fillRect(x, y, w, h, CLR_BG);
        _tft.drawRect(x, y, w, h, color);
        _tft.drawFastHLine(x + 2, y + 14, w - 4, 0x2104);
        _tft.setTextSize(1);
        _tft.setTextColor(CLR_LABEL, CLR_PANEL);
        _tft.setCursor(x + 4, y + 4);
        _tft.print(t.title);
        c.valid = true;
        c.color = color;
        c.hdr[0] = c.value[0] = c.row1[0] = c.row2[0] = '\0';
        c.soc   = -1;
    }

    const bool    sparse = t.row2[0] == '\0' && t.soc < 0;
    const int16_t ix = x + 4, iw = w - 8;
    drawCached(c.hdr,   ix, iw, AL_RIGHT,  y + 4, 1, color, CLR_PANEL, t.hdr);
    drawCached(c.value, ix, iw, AL_CENTER, sparse ? y + 24 : y + 19, 2,
               color, CLR_PANEL, t.value);
    drawCached(c.row1,  ix, iw, AL_CENTER, sparse ? y + 54 : y + 43, 1,
               CLR_LABEL, CLR_PANEL, t.row1);

    if (t.soc >= 0) {
        if (c.soc != t.soc) {
            c.soc = t.soc;
            int16_t bx = x + 6, by = y + 58, bw = w - 12, bh = 14;
            _tft.drawRect(bx, by, bw, bh, CLR_LABEL);
            int16_t innerMax = bw - 4;
            int16_t fillW    = (int16_t)((int32_t)innerMax * t.soc / 100);
            uint16_t barClr  = t.soc >= 50 ? CLR_ACTIVE
                             : (t.soc >= 20 ? CLR_PV : CLR_IMPORT);
            if (fillW > 0)
                _tft.fillRect(bx + 2, by + 2, fillW, bh - 4, barClr);
            if (fillW < innerMax)
                _tft.fillRect(bx + 2 + fillW, by + 2, innerMax - fillW, bh - 4, CLR_BG);
        }
    } else {
        drawCached(c.row2, ix, iw, AL_CENTER, y + 58, 1, CLR_LABEL, CLR_PANEL, t.row2);
    }
}

void Display::redrawHub(const HubText& t) {
    uint16_t border = t.passiveOk ? CLR_TARGET : CLR_IMPORT;
    if (!_hubInv.valid || _hubInv.color != border) {
        _tft.fillRect(HUB_X, HUB_Y, HUB_W, HUB_H, CLR_BG);
        _tft.drawRect(HUB_X, HUB_Y, HUB_W, HUB_H, border);
        _hubInv.valid = true;
        _hubInv.color = border;
        _hubInv.line1[0] = _hubInv.line2[0] = '\0';
    }
    drawCached(_hubInv.line1, HUB_X, HUB_W, AL_CENTER, HUB_Y + 7,  1,
               CLR_LABEL,  CLR_BG, t.line1);
    drawCached(_hubInv.line2, HUB_X, HUB_W, AL_CENTER, HUB_Y + 23, 1,
               CLR_TARGET, CLR_BG, t.line2);
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

void Display::redrawButton(const ButtonText& b) {
    uint16_t bg = b.active ? 0x03E0 : 0x4000;
    if (!_btnValid || _btnActive != b.active) {
        _tft.fillRect(BTN_X, BTN_Y, BTN_W, BTN_H, bg);
        _tft.drawRect(BTN_X, BTN_Y, BTN_W, BTN_H, b.active ? CLR_ACTIVE : CLR_IMPORT);
        _btnValid  = true;
        _btnActive = b.active;
        _btnLine1[0] = _btnLine2[0] = '\0';   // force text repaint
    }
    uint16_t fg = b.active ? ILI9341_WHITE : 0xBDF7;
    drawCached(_btnLine1, BTN_X, BTN_W, AL_CENTER, BTN_Y + 6,  2, fg, bg, b.line1);
    drawCached(_btnLine2, BTN_X, BTN_W, AL_CENTER, BTN_Y + 28, 1, CLR_TARGET, bg, b.line2);
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
    for (auto& l : _sysCache) l[0] = '\0';
    _dotsValid = false;
    _logValid  = false;
}

void Display::update(const InverterData& inv, const UiStatus& ui) {
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

    drawTopLinkDots(ui.wifiOk, ui.modbusOk, ui.mqttOk);

    if (_tab == TAB_FLOW) updateFlow(inv, ui);
    else                  updateSys(inv, ui);
}

void Display::updateFlow(const InverterData& inv, const UiStatus& ui) {
    CardText c;
    flowSolar(inv, c); redrawCard(CD_LX, CD_TY, c, _cdSolar);
    flowGrid(inv, c);  redrawCard(CD_RX, CD_TY, c, _cdGrid);
    flowHome(inv, c);  redrawCard(CD_LX, CD_BY, c, _cdHome);
    flowBatt(inv, c);  redrawCard(CD_RX, CD_BY, c, _cdBatt);

    HubText h;
    flowHub(inv, h);
    redrawHub(h);

    FlowArms a;
    flowArms(inv, a);
    uint16_t gc = toneColor(inv.gridPower    >= 0 ? Tone::Export : Tone::Import);
    uint16_t bc = toneColor(inv.batteryPower >= 0 ? Tone::Charge : Tone::Discharge);
    redrawArm(0, a.pv,   true,        CLR_PV,        _arPv);
    redrawArm(1, a.grid, a.gridToHub, gc,            _arGrid);
    redrawArm(2, a.home, false,       ILI9341_WHITE, _arHome);
    redrawArm(3, a.batt, a.battToHub, bc,            _arBatt);

    ButtonText b;
    flowButton(inv, ui, b);
    redrawButton(b);
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
        char ch = appLog.at(i);
        if (ch != '\n') {
            if (col < sizeof(row) - 1) row[col++] = ch;
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

void Display::updateSys(const InverterData& inv, const UiStatus& ui) {
    char lines[FM_SYS_LINES][64];
    sysLines(inv, ui, lines);
    for (uint8_t i = 0; i < FM_SYS_LINES; i++)
        drawCached(_sysCache[i], 8, 232, AL_LEFT, SYS_Y0 + i * SYS_DY, 1,
                   CLR_LABEL, CLR_BG, lines[i]);

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
    _touch.startCalibration();
    _calDrawn = false;
    if (!_screenOn) wake();
}

void Display::drawCalScreen() {
    static const int16_t tx[3] = {TCAL_LO_X, TCAL_HI_X, TCAL_LO_X};
    static const int16_t ty[3] = {TCAL_LO_Y, TCAL_LO_Y, TCAL_HI_Y};
    uint8_t step = _touch.calStep();
    _tft.fillScreen(CLR_BG);
    drawCentered(120, "TOUCH CALIBRATION", 2, CLR_PV);
    char msg[32];
    snprintf(msg, sizeof(msg), "tap the crosshair  %d of 3", step + 1);
    drawCentered(150, msg, 1, CLR_LABEL);
    if (_touch.calRetry())
        drawCentered(170, "bad samples - starting over", 1, CLR_IMPORT);

    int16_t x = tx[step], y = ty[step];
    _tft.drawCircle(x, y, 10, CLR_LABEL);
    _tft.drawCircle(x, y, 3, CLR_ACTIVE);
    _tft.drawFastHLine(x - 16, y, 33, CLR_ACTIVE);
    _tft.drawFastVLine(x, y - 16, 33, CLR_ACTIVE);
    _calDrawn = true;
}

bool Display::pollTouch() {
    uint8_t ev = _touch.poll(_screenOn);
    if (ev == 0) return false;
    if (ev & TouchInput::TE_WAKE) { wake(); return false; }
    if (ev & TouchInput::TE_CAL_START) {
        _calDrawn = false;
        if (!_screenOn) wake();
        return false;
    }
    if (ev & TouchInput::TE_CAL_REDRAW) _calDrawn = false;
    if (ev & TouchInput::TE_CAL_DONE)   _needFullDraw = true;
    if (ev & TouchInput::TE_LONG) {                  // long press: switch tab
        setTab(_tab == TAB_FLOW ? TAB_SYS : TAB_FLOW);
        appLog.add("TCH", _tab == TAB_SYS ? "long press -> SYS"
                                          : "long press -> FLOW");
    }
    if (ev & TouchInput::TE_HINT) drawHint();
    if (!(ev & TouchInput::TE_TAP)) return false;

    int16_t tx = _touch.tapX(), ty = _touch.tapY();
    if (_touch.cal().valid && tx >= 0) {             // calibrated: use regions
        if (ty < TAB_H) {
            setTab(tx < TAB_FLOW_X1 ? TAB_FLOW : TAB_SYS);
            return false;
        }
        bool inBtn = _tab == TAB_FLOW && tx >= BTN_X && tx < BTN_X + BTN_W
                     && ty >= BTN_Y && ty < BTN_Y + BTN_H;
        if (inBtn) appLog.add("TCH", "button -> toggle saver");
        return inBtn;
    }
    if (_tab != TAB_FLOW) return false;              // uncalibrated fallback
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
    if ((millis() - _touch.lastTouchAt()) > SCREEN_DIM_MS) {
        if (millis() - _lastDimStep < 40) return;   // rate-limit, no blocking
        _lastDimStep = millis();
        _brightness--;
        analogWrite(PIN_TFT_LED, _brightness);
        if (_brightness == 0) _screenOn = false;
    }
}
