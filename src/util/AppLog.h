#ifndef SOFAR_APPLOG_H
#define SOFAR_APPLOG_H

#include <Arduino.h>

class AppLog {
public:
    void add(const char* tag, const char* msg);
    void add(const char* tag, const String& msg) { add(tag, msg.c_str()); }

    String text() const;

    // Monotonic counter, bumped on every add() — lets consumers redraw
    // only when the log actually changed.
    unsigned int serial() const { return _serial; }

    unsigned int used() const { return _len; }

private:
    static constexpr unsigned int BUF_SZ = 2048;
    char         _buf[BUF_SZ];
    unsigned int _head   = 0;
    unsigned int _len    = 0;
    unsigned int _serial = 0;

    void push(char c);
    void pushStr(const char* s);
};

extern AppLog appLog;

#endif
