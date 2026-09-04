#ifndef SOFAR_DISPLAY_H
#define SOFAR_DISPLAY_H

#include <Adafruit_ILI9341.h>
#include <XPT2046_Touchscreen.h>

struct InverterData;
class  BatterySaver;
class  ModeController;

class Display {
public:
    Display();

    void begin();

    // Show a two-line centred message (used during boot).
    void showSplash(const char* line1, const char* line2);

    // Redraw the LCD (internally rate-limited).  Two tabs:
    // FLOW = power-flow diagram, SYS = system overview + log.
    void update(const InverterData& inv, const BatterySaver& bs,
                const ModeController& ctrl, const char* serialNumber,
                bool wifiOk, bool modbusOk, bool mqttOk);

    // Touch handling.  Returns true only on a tap of the saver button in
    // the FLOW tab (taps on the tab bar switch tabs instead).
    bool pollTouch();

    // Gradually dim after timeout; wake on touch.
    void handleDimming();

private:
    enum Tab : uint8_t { TAB_FLOW = 0, TAB_SYS = 1 };

    Adafruit_ILI9341      _tft;
    XPT2046_Touchscreen   _ts;

    Tab           _tab          = TAB_FLOW;
    uint8_t       _brightness   = 0;
    bool          _screenOn     = false;
    bool          _touchedPrev  = false;
    unsigned long _lastTouch    = 0;
    unsigned long _lastRedraw   = 0;
    unsigned long _lastDimStep  = 0;
    bool          _needFullDraw = true;
    const char*   _sn           = "";
    char          _touchDbg[40] = "";   // last tap, raw -> screen coords

    // Differential-redraw caches: text/lines are repainted only when their
    // content actually changed (plain full redraws roll visibly over SPI).
    struct NodeCache { char value[20] = ""; char sub[28] = ""; uint16_t color = 0; bool valid = false; };
    struct ArrowCache { char label[12] = ""; bool dir = false; bool drawn = false; };
    struct ArrowDef { int16_t fx, fy, tx, ty, lx, ly; };
    static constexpr uint8_t SYS_LINES = 5;
    static constexpr uint8_t LOG_LINES_MAX = 19;   // ~8 px per line below y=158

    NodeCache  _ndSolar, _ndGrid, _ndBatt, _ndHome;
    ArrowCache _arPv, _arGrid, _arBatt;
    char       _btnLine1[20] = "";
    char       _btnLine2[36] = "";
    bool       _btnValid     = false;
    bool       _btnActive    = false;
    char       _sysCache[5][64];
    bool       _sysValid  = false;
    uint8_t    _dotsCache = 0xFF;
    bool       _dotsValid = false;
    unsigned int _logSerial = 0;
    bool         _logValid  = false;

    void setTab(Tab t);
    void invalidateCaches();
    void drawTabBar();
    void drawNodeFrame(int16_t x, int16_t y, int16_t w, int16_t h,
                       const char* name);
    void redrawNode(int16_t x, int16_t y, int16_t w, int16_t h,
                    const char* name, const char* value, const char* sub,
                    uint16_t color, NodeCache& c);
    void drawArrowLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                       uint16_t color);
    void redrawArrow(const ArrowDef& d, bool forward, uint16_t color,
                     const char* label, ArrowCache& c);
    void printOver(int16_t x0, int16_t x1, int16_t y, uint8_t size,
                   uint16_t fg, uint16_t bg, const char* prev, const char* next);
    void updateFlow(const InverterData& inv, const BatterySaver& bs,
                    const ModeController& ctrl);
    void updateSys(const InverterData& inv, const char* sn,
                   bool wifiOk, bool modbusOk, bool mqttOk);
    void drawCentered(int16_t y, const String& text, uint8_t size,
                      uint16_t fg);
    void sysPrint(uint8_t idx, int16_t y, const char* text);
    void drawStatusDots(bool wifiOk, bool modbusOk, bool mqttOk);
    void drawLogTail(int16_t y);
    void wake();
};

#endif
