#include "Display.h"
#include <ESP8266WiFi.h>
#include "inverter/Inverter.h"
#include "control/BatterySaver.h"

// ── Colour palette ─────────────────────────────────────────────
#define CLR_BG       ILI9341_BLACK
#define CLR_LABEL    0xBDF7            // light grey
#define CLR_EXPORT   ILI9341_GREEN
#define CLR_IMPORT   ILI9341_RED
#define CLR_CHARGE   0x04BF            // sky blue
#define CLR_DISCHG   ILI9341_ORANGE
#define CLR_PV       ILI9341_YELLOW
#define CLR_TARGET   ILI9341_CYAN
#define CLR_ACTIVE   ILI9341_GREEN
#define CLR_OFF      0x8410            // dim grey
#define CLR_DIVIDER  0x4208

// Layout constants (240 × 320 portrait)
#define HDR_Y         0
#define HDR_H        44
#define ROW_START    52
#define ROW_H        32
#define BTN_Y       220
#define BTN_H        50
#define BTN_X         8
#define BTN_W       224
#define STATUS_Y    278

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

// ── Splash screen (shown during boot) ────────────────────────────
void Display::showSplash(const char* line1, const char* line2) {
    _tft.fillScreen(CLR_BG);
    drawCentered(120, String(line1), 2, ILI9341_WHITE);
    drawCentered(150, String(line2), 2, CLR_PV);
}

// ── Static chrome (header bar, dividers, labels) ───────────────
void Display::drawChrome() {
    _tft.fillScreen(CLR_BG);

    // Header background
    _tft.fillRect(0, HDR_Y, 240, HDR_H, 0x0010);  // dark blue
    _tft.setTextSize(2);
    _tft.setTextColor(ILI9341_WHITE, 0x0010);
    _tft.setCursor(16, 14);
    _tft.print("BATTERY SAVER");

    // Dividers
    _tft.drawFastHLine(0, HDR_H + 2, 240, CLR_DIVIDER);
    _tft.drawFastHLine(0, BTN_Y - 4, 240, CLR_DIVIDER);
    _tft.drawFastHLine(0, STATUS_Y - 2, 240, CLR_DIVIDER);
}

// ── Draw one metric row ────────────────────────────────────────
void Display::drawRow(int16_t y, const char* label, int32_t value,
                      const char* unit, uint16_t color)
{
    // Clear row
    _tft.fillRect(0, y, 240, ROW_H - 2, CLR_BG);

    // Label (left-aligned)
    _tft.setTextSize(2);
    _tft.setTextColor(CLR_LABEL, CLR_BG);
    _tft.setCursor(8, y + 6);
    _tft.print(label);

    // Value (right-aligned)
    String val = String(value) + " " + unit;
    int16_t w = val.length() * 12;   // size-2 char = 12 px wide
    _tft.setTextColor(color, CLR_BG);
    _tft.setCursor(232 - w, y + 6);
    _tft.print(val);
}

// ── Centred text helper ────────────────────────────────────────
void Display::drawCentered(int16_t y, const String& text, uint8_t size,
                           uint16_t fg)
{
    int16_t w = text.length() * 6 * size;
    _tft.setTextSize(size);
    _tft.setTextColor(fg, CLR_BG);
    _tft.setCursor((240 - w) / 2, y);
    _tft.print(text);
}

// ── Main update ────────────────────────────────────────────────
void Display::update(const InverterData& inv, const BatterySaver& bs,
                     bool wifiOk, bool modbusOk, bool mqttOk)
{
    if (!_screenOn) return;   // skip SPI writes while backlight is off

    unsigned long now = millis();
    if (!_needFullDraw && (now - _lastRedraw < INTERVAL_DISPLAY)) return;
    _lastRedraw = now;

    if (_needFullDraw) {
        drawChrome();
        _needFullDraw = false;
        // Force all rows to redraw
        _prevGrid = -99999; _prevBatt = -99999; _prevSOC = 9999;
        _prevPV   = -1;     _prevLoad = -99999; _prevTarget = -99999;
        _prevActive = !bs.isActive();
    }

    int16_t row = ROW_START;

    // Grid
    if (inv.gridPower != _prevGrid) {
        uint16_t clr = inv.gridPower >= 0 ? CLR_EXPORT : CLR_IMPORT;
        drawRow(row, "Grid", inv.gridPower, "W", clr);
        _prevGrid = inv.gridPower;
    }
    row += ROW_H;

    // PV
    if (inv.pvPower != _prevPV) {
        drawRow(row, "Solar", inv.pvPower, "W", CLR_PV);
        _prevPV = inv.pvPower;
    }
    row += ROW_H;

    // Battery
    if (inv.batteryPower != _prevBatt) {
        uint16_t clr = inv.batteryPower >= 0 ? CLR_CHARGE : CLR_DISCHG;
        drawRow(row, "Batt", inv.batteryPower, "W", clr);
        _prevBatt = inv.batteryPower;
    }
    row += ROW_H;

    // SOC
    if (inv.batterySOC != _prevSOC) {
        uint16_t clr = inv.batterySOC > 20 ? CLR_CHARGE : CLR_IMPORT;
        drawRow(row, "SOC", inv.batterySOC, "%", clr);
        _prevSOC = inv.batterySOC;
    }
    row += ROW_H;

    // Load
    if (inv.loadPower != _prevLoad) {
        drawRow(row, "Load", inv.loadPower, "W", ILI9341_WHITE);
        _prevLoad = inv.loadPower;
    }

    // Toggle button
    if (bs.isActive() != _prevActive || bs.targetPower() != _prevTarget) {
        drawButton(bs.isActive(), bs.targetPower());
        _prevActive = bs.isActive();
        _prevTarget = bs.targetPower();
    }

    // Status bar
    _tft.fillRect(0, STATUS_Y, 240, 42, CLR_BG);
    _tft.setTextSize(1);
    _tft.setTextColor(CLR_LABEL, CLR_BG);
    // WiFi
    _tft.fillCircle(12, STATUS_Y + 6, 5, wifiOk ? CLR_ACTIVE : CLR_IMPORT);
    _tft.setCursor(22, STATUS_Y + 2);
    _tft.print("WiFi");
    // RS485
    _tft.fillCircle(62, STATUS_Y + 6, 5, modbusOk ? CLR_ACTIVE : CLR_IMPORT);
    _tft.setCursor(72, STATUS_Y + 2);
    _tft.print("RS485");
    // MQTT
    _tft.fillCircle(118, STATUS_Y + 6, 5, mqttOk ? CLR_ACTIVE : CLR_IMPORT);
    _tft.setCursor(128, STATUS_Y + 2);
    _tft.print("MQTT");
    // IP address
    if (wifiOk) {
        _tft.setTextColor(CLR_LABEL, CLR_BG);
        _tft.setCursor(8, STATUS_Y + 20);
        _tft.print(WiFi.localIP().toString());
    }
}

// ── Draw toggle button ────────────────────────────────────────
void Display::drawButton(bool active, int32_t target) {
    uint16_t bg  = active ? 0x03E0 : 0x4000;   // dark green / dark red
    uint16_t fg  = active ? ILI9341_WHITE : 0xBDF7;
    _tft.fillRect(BTN_X, BTN_Y, BTN_W, BTN_H, bg);
    _tft.drawRect(BTN_X, BTN_Y, BTN_W, BTN_H, active ? CLR_ACTIVE : CLR_IMPORT);

    _tft.setTextSize(2);
    _tft.setTextColor(fg, bg);
    const char* label = active ? "BATT SAVE  ON" : "BATT SAVE OFF";
    int16_t tw = strlen(label) * 12;
    _tft.setCursor(BTN_X + (BTN_W - tw) / 2, BTN_Y + 8);
    _tft.print(label);

    if (active) {
        _tft.setTextSize(1);
        _tft.setTextColor(CLR_TARGET, bg);
        String sub = "Target: " + String(target) + " W";
        int16_t sw = sub.length() * 6;
        _tft.setCursor(BTN_X + (BTN_W - sw) / 2, BTN_Y + 32);
        _tft.print(sub);
    }
}

// ── Touch polling ──────────────────────────────────────────────
bool Display::pollTouch() {
    if (!_ts.tirqTouched()) { _touchedPrev = false; return false; }
    if (!_ts.touched())     { _touchedPrev = false; return false; }
    if (_touchedPrev)       return false;   // de-bounce: one event per press

    _touchedPrev = true;
    _lastTouch   = millis();

    if (!_screenOn) { wake(); return false; }   // first tap wakes screen
    return true;                                // tap while awake = toggle
}

// ── Dimming ────────────────────────────────────────────────────
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
        if (_brightness > 0) { _brightness--; analogWrite(PIN_TFT_LED, _brightness); delay(40); }
        if (_brightness == 0) _screenOn = false;
    }
}
