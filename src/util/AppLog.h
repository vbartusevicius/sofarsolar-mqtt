#ifndef SOFAR_APPLOG_H
#define SOFAR_APPLOG_H

#include <Arduino.h>

class AppLog {
public:
    void add(const char* tag, const char* msg);
    void add(const char* tag, const String& msg) { add(tag, msg.c_str()); }

    // bumped on every add(); lets consumers redraw only when the log changed
    unsigned int serial() const { return _serial; }

    unsigned int used() const { return _len; }

    // Logical character access, oldest first, 0 .. end()-1
    char at(unsigned int i) const {
        return _len < BUF_SZ ? _buf[i] : _buf[(_head + i) % BUF_SZ];
    }

    // One past the last character, with trailing newlines trimmed
    unsigned int end() const;

    // Index of the first character of the last `maxLines` lines
    unsigned int tailStart(unsigned int maxLines) const;

private:
    static constexpr unsigned int BUF_SZ = 2048;
    // Flush a "repeated" line at least this often during a long run, so the
    // log shows the problem is ongoing rather than one old event.
    static constexpr unsigned int REPEAT_FLUSH = 100;
    char         _buf[BUF_SZ];
    unsigned int _head   = 0;
    unsigned int _len    = 0;
    unsigned int _serial = 0;
    // Identical consecutive entries are counted, not stored: an inverter
    // asleep overnight fails the same read every second, which used to push
    // every useful line out of the 2 kB ring within minutes.
    uint32_t     _lastHash = 0;
    unsigned int _repeats  = 0;

    void push(char c);
    void pushStr(const char* s);
    void pushEntry(const char* tag, const char* msg);
    void flushRepeats();
    static uint32_t hash(const char* tag, const char* msg);
};

extern AppLog appLog;

#endif
