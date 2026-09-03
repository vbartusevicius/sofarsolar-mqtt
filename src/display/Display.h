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

    void setTab(Tab t);
    void drawTabBar();
    void drawNode(int16_t x, int16_t y, int16_t w, int16_t h,
                  const char* name, const char* value, const char* sub,
                  uint16_t color);
    void drawArrow(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                   uint16_t color, const char* label);
    void drawSaverButton(const BatterySaver& bs, const ModeController& ctrl);
    void updateFlow(const InverterData& inv, const BatterySaver& bs,
                    const ModeController& ctrl);
    void updateSys(const InverterData& inv, const char* sn,
                   bool wifiOk, bool modbusOk, bool mqttOk);
    void drawCentered(int16_t y, const String& text, uint8_t size,
                      uint16_t fg);
    void sysLine(int16_t& y, const char* label, const char* value,
                 uint16_t color, bool twoCol, const char* label2,
                 const char* value2, uint16_t color2);
    void drawStatusDots(bool wifiOk, bool modbusOk, bool mqttOk);
    void drawLogTail(int16_t y);
    void wake();
};

#endif
