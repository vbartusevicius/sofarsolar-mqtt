#ifndef SOFAR_DISPLAY_H
#define SOFAR_DISPLAY_H

#include <Adafruit_ILI9341.h>
#include <XPT2046_Touchscreen.h>
#include "display/TouchCal.h"

struct InverterData;
class  BatterySaver;
class  ModeController;

class Display {
public:
    Display();

    void begin();
    void showSplash(const char* line1, const char* line2);

    // FLOW tab = power-flow diagram, SYS tab = system overview + log
    void update(const InverterData& inv, const BatterySaver& bs,
                const ModeController& ctrl, const char* serialNumber,
                bool wifiOk, bool modbusOk, bool mqttOk);

    bool pollTouch();
    void handleDimming();
    void startCalibration();
    void setTouchCal(const TouchCal& c) { _cal = c; }
    const TouchCal& touchCal() const    { return _cal; }
    bool calibrating() const            { return _calStep < CAL_DONE; }
    void onCalibrated(void (*cb)(const TouchCal&)) { _calSaved = cb; }

private:
    enum Tab : uint8_t { TAB_FLOW = 0, TAB_SYS = 1 };
    enum CalStep : uint8_t { CAL_A = 0, CAL_B, CAL_C, CAL_DONE };

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
    char          _touchDbg[40] = "";
    unsigned long _pressStart   = 0;
    unsigned long _lastPollAt   = 0;
    unsigned long _lastDownAt   = 0;
    bool          _calNotice    = false;
    bool          _longFired    = false;

    TouchCal      _cal;
    uint8_t       _calStep      = CAL_DONE;
    bool          _calDrawn     = false;
    bool          _calRetry     = false;
    bool          _calIgnorePress = false;
    TouchSample   _calRaw[3]    = {};
    int32_t       _calSumX      = 0;
    int32_t       _calSumY      = 0;
    int16_t       _calCount     = 0;
    void (*_calSaved)(const TouchCal&) = nullptr;
    int16_t       _tapX         = -1;   // last press in screen pixels, -1 if unknown
    int16_t       _tapY         = -1;

    // Repaint only changed content — full redraws roll visibly over SPI
    struct NodeCache { char value[20] = ""; char sub[28] = ""; uint16_t color = 0; bool valid = false; };
    struct ArrowCache { char label[12] = ""; bool dir = false; bool drawn = false; };
    struct ArrowDef { int16_t fx, fy, tx, ty, lx, ly; };
    static constexpr uint8_t SYS_LINES = 5;
    static constexpr uint8_t LOG_LINES_MAX = 17;

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
    void drawHint();
    void drawHintLine(const char* text);
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
    void drawCalScreen();
    bool calPollTouch(bool down, unsigned long now);
    void wake();
};

#endif
