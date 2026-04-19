#ifndef SOFAR_APPLOG_H
#define SOFAR_APPLOG_H

#include <Arduino.h>

// Simple circular text log accessible from any module.
// Keeps the last ~2 KB of timestamped entries in RAM.
class AppLog {
public:
    void add(const char* tag, const String& msg) {
        String line = String(millis() / 1000) + "s [" + tag + "] " + msg + "\n";
        _buf += line;
        if (_buf.length() > MAX_LEN) _buf = _buf.substring(_buf.length() - TRIM_TO);
    }

    const String& text() const { return _buf; }

private:
    static constexpr unsigned int MAX_LEN = 2048;
    static constexpr unsigned int TRIM_TO = 1536;
    String _buf;
};

// Single global instance – defined in AppLog.cpp
extern AppLog appLog;

#endif // SOFAR_APPLOG_H
