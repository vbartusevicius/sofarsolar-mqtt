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
    // Timestamp
    char tmp[12];
    ultoa(millis() / 1000, tmp, 10);
    pushStr(tmp);
    pushStr("s [");
    pushStr(tag);
    pushStr("] ");
    pushStr(msg);
    push('\n');
}

String AppLog::text() const {
    String out;
    out.reserve(_len);
    if (_len < BUF_SZ) {
        // Buffer hasn't wrapped yet — data starts at 0
        for (unsigned int i = 0; i < _len; i++) out += _buf[i];
    } else {
        // Wrapped — oldest data starts at _head
        for (unsigned int i = 0; i < BUF_SZ; i++)
            out += _buf[(_head + i) % BUF_SZ];
    }
    return out;
}
