#ifndef SOFAR_DISPLAY_H
#define SOFAR_DISPLAY_H

#include <Adafruit_ILI9341.h>
#include "display/FlowModel.h"
#include "display/TouchCal.h"
#include "display/TouchInput.h"

struct InverterData;

// Draws what FlowModel says; TouchInput turns presses into events. Display
// depends on no control or network class: the caller passes a UiStatus.
class Display {
public:
    Display();

    void begin();
    void showSplash(const char* line1, const char* line2);

    // FLOW tab = power-flow diagram, SYS tab = system overview + log
    void update(const InverterData& inv, const UiStatus& ui);

    // true when the user asked to toggle the battery saver
    bool pollTouch();
    void handleDimming();
    void startCalibration();
    void setTouchCal(const TouchCal& c) { _touch.setCal(c); }
    const TouchCal& touchCal() const    { return _touch.cal(); }
    bool calibrating() const            { return _touch.calibrating(); }
    void onCalibrated(void (*cb)(const TouchCal&)) { _touch.onCalibrated(cb); }

private:
    enum Tab : uint8_t { TAB_FLOW = 0, TAB_SYS = 1 };
    enum Align : uint8_t { AL_LEFT, AL_CENTER, AL_RIGHT };

    Adafruit_ILI9341 _tft;
    TouchInput       _touch;

    Tab           _tab          = TAB_FLOW;
    uint8_t       _brightness   = 0;
    bool          _screenOn     = false;
    unsigned long _lastRedraw   = 0;
    unsigned long _lastDimStep  = 0;
    bool          _needFullDraw = true;
    bool          _calDrawn     = false;

    // Repaint only changed content — full redraws roll visibly over SPI.
    // An empty cache string means "nothing drawn there yet".
    struct CardCache {
        char     hdr[14]   = "";
        char     value[16] = "";
        char     row1[20]  = "";
        char     row2[20]  = "";
        int8_t   soc       = -1;
        uint16_t color     = 0;
        bool     valid     = false;
    };
    struct HubCache {
        char     line1[16] = "";
        char     line2[16] = "";
        uint16_t color     = 0;
        bool     valid     = false;
    };
    struct ArrowCache { bool dir = false; bool drawn = false; uint16_t color = 0; };

    CardCache  _cdSolar, _cdGrid, _cdHome, _cdBatt;
    HubCache   _hubInv;
    ArrowCache _arPv, _arGrid, _arHome, _arBatt;
    char       _btnLine1[20] = "";
    char       _btnLine2[40] = "";
    bool       _btnValid     = false;
    bool       _btnActive    = false;
    char       _sysCache[FM_SYS_LINES][64];
    uint8_t    _dotsCache = 0xFF;
    bool       _dotsValid = false;
    unsigned int _logSerial = 0;
    bool         _logValid  = false;

    static uint16_t toneColor(Tone t);
    static int16_t  alignX(int16_t x, int16_t w, Align a, const char* s, uint8_t size);

    // Erases the cached string and prints `next` in its place, if it changed
    template <size_t N>
    void drawCached(char (&cache)[N], int16_t x, int16_t w, Align a, int16_t y,
                    uint8_t size, uint16_t fg, uint16_t bg, const char* next) {
        if (strcmp(cache, next) == 0) return;
        printOver(alignX(x, w, a, cache, size), alignX(x, w, a, next, size),
                  y, size, fg, bg, cache, next);
        strncpy(cache, next, N - 1);
        cache[N - 1] = '\0';
    }

    void setTab(Tab t);
    void invalidateCaches();
    void drawTabBar();
    void drawTopLinkDots(bool wifiOk, bool modbusOk, bool mqttOk);
    void drawHint();
    void drawHintLine(const char* text);
    void redrawCard(int16_t x, int16_t y, const CardText& t, CardCache& c);
    void redrawHub(const HubText& t);
    void drawElbowArm(uint8_t arm, bool toHub, uint16_t color);
    void redrawArm(uint8_t arm, bool visible, bool toHub,
                   uint16_t color, ArrowCache& c);
    void redrawButton(const ButtonText& b);
    void printOver(int16_t x0, int16_t x1, int16_t y, uint8_t size,
                   uint16_t fg, uint16_t bg, const char* prev, const char* next);
    void updateFlow(const InverterData& inv, const UiStatus& ui);
    void updateSys(const InverterData& inv, const UiStatus& ui);
    void drawCentered(int16_t y, const char* text, uint8_t size, uint16_t fg);
    void drawLogTail(int16_t y);
    void drawCalScreen();
    void wake();
};

#endif
