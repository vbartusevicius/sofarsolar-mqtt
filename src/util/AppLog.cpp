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

unsigned int AppLog::end() const {
    unsigned int e = _len;
    while (e > 0 && at(e - 1) == '\n') e--;
    return e;
}

unsigned int AppLog::tailStart(unsigned int maxLines) const {
    if (maxLines == 0) return end();
    unsigned int e = end(), seen = 0;
    for (unsigned int i = e; i > 0; i--) {
        if (at(i - 1) != '\n') continue;
        if (++seen >= maxLines) return i;
    }
    return 0;
}
