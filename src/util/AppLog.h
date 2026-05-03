#ifndef SOFAR_APPLOG_H
#define SOFAR_APPLOG_H

#include <Arduino.h>

class AppLog {
public:
    void add(const char* tag, const String& msg);

    String text() const;

    unsigned int used() const { return _len; }

private:
    static constexpr unsigned int BUF_SZ = 2048;
    char         _buf[BUF_SZ];
    unsigned int _head = 0;
    unsigned int _len  = 0;

    void push(char c);
    void pushStr(const char* s);
};

extern AppLog appLog;

#endif
