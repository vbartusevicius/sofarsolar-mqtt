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

uint32_t AppLog::hash(const char* tag, const char* msg) {
    uint32_t h = 2166136261UL;                       // FNV-1a
    for (const char* p = tag; *p; p++) h = (h ^ (uint8_t)*p) * 16777619UL;
    h = (h ^ 0xFF) * 16777619UL;                     // tag/msg separator
    for (const char* p = msg; *p; p++) h = (h ^ (uint8_t)*p) * 16777619UL;
    return h;
}

void AppLog::pushEntry(const char* tag, const char* msg) {
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

void AppLog::flushRepeats() {
    if (_repeats == 0) return;
    char msg[40];
    snprintf(msg, sizeof(msg), "last message repeated %ux", _repeats);
    _repeats = 0;
    pushEntry("LOG", msg);
}

void AppLog::add(const char* tag, const char* msg) {
    uint32_t h = hash(tag, msg);
    if (_serial > 0 && h == _lastHash) {
        if (++_repeats >= REPEAT_FLUSH) flushRepeats();
        return;
    }
    flushRepeats();
    _lastHash = h;
    pushEntry(tag, msg);
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
