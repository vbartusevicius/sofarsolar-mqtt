#ifndef SOFAR_DISPLAY_H
#define SOFAR_DISPLAY_H

#include <Adafruit_ILI9341.h>
#include <XPT2046_Touchscreen.h>
#include "Config.h"

struct InverterData;
class  BatterySaver;

class Display {
public:
    Display();

    void begin();

    // Redraw the dashboard (internally rate-limited).
    void update(const InverterData& inv, const BatterySaver& bs,
                bool wifiOk, bool modbusOk, bool mqttOk);

    // Returns true once per tap in the toggle zone.
    bool pollTouch();

    // Gradually dim after timeout; wake on touch.
    void handleDimming();

private:
    Adafruit_ILI9341      _tft;
    XPT2046_Touchscreen   _ts;

    uint8_t       _brightness   = 0;
    bool          _screenOn     = false;
    bool          _touchedPrev  = false;
    unsigned long _lastTouch    = 0;
    unsigned long _lastRedraw   = 0;
    bool          _needFullDraw = true;

    // Cached values for partial-redraw optimisation
    int32_t  _prevGrid   = -99999;
    int32_t  _prevBatt   = -99999;
    uint16_t _prevSOC    = 9999;
    int32_t  _prevPV     = -1;
    int32_t  _prevLoad   = -99999;
    int32_t  _prevTarget = -99999;
    bool     _prevActive = false;

    void drawChrome();
    void drawRow(int16_t y, const char* label, int32_t value,
                 const char* unit, uint16_t color);
    void drawButton(bool active, int32_t target);
    void drawCentered(int16_t y, const String& text, uint8_t size,
                      uint16_t fg);
    void wake();
};

#endif // SOFAR_DISPLAY_H
