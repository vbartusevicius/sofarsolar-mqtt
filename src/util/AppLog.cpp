#include "util/AppLog.h"

AppLog appLog;

void AppLog::push(char c) {
    _buf[_head] = c;
    _head = (_head + 1) % BUF_SZ;
    if (_len < BUF_SZ) _len++;
}

void AppLog::pushStr(const char* s) {
    while (*s) push(*s++);
}

void AppLog::add(const char* tag, const char* msg) {
    char tmp[12];
    ultoa(millis() / 1000, tmp, 10);
    pushStr(tmp);
    pushStr("s [");
    pushStr(tag);
    pushStr("] ");
    pushStr(msg);
    push('\n');
    _serial++;
}

String AppLog::text() const {
    String out;
    out.reserve(_len);
    if (_len < BUF_SZ) {
        for (unsigned int i = 0; i < _len; i++) out += _buf[i];
    } else {
        for (unsigned int i = 0; i < BUF_SZ; i++)   // wrapped: oldest at _head
            out += _buf[(_head + i) % BUF_SZ];
    }
    return out;
}
