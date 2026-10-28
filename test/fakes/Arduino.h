#ifndef FAKE_ARDUINO_H
#define FAKE_ARDUINO_H

// Minimal Arduino.h fake for native unit tests (only what src/ headers need
// transitively). Extend as needed.
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <vector>

typedef bool boolean;
typedef uint8_t byte;

#ifndef PROGMEM
#define PROGMEM
#endif

class String {
public:
    String(const char* s = "") : _s(s ? s : "") {}
    const char* c_str() const { return _s; }
private:
    const char* _s;
};

struct FakeClock {
    unsigned long now = 1000;
};
inline FakeClock& fakeClock() {
    static FakeClock c;
    return c;
}

inline unsigned long millis() { return fakeClock().now; }
inline void delay(unsigned long ms) { fakeClock().now += ms; }
inline void yield() {}

inline char* ultoa(unsigned long val, char* s, int) {
    snprintf(s, 12, "%lu", val);
    return s;
}

// AVR/ESP libc extension: caller's buffer must hold the result (16 B is ample)
inline char* dtostrf(double val, signed char width, unsigned char prec, char* s) {
    snprintf(s, 16, "%*.*f", (int)width, (int)prec, val);
    return s;
}

struct FakeSerial {
    std::vector<uint8_t> rx;
    size_t               rxPos = 0;
    std::vector<uint8_t> tx;
    uint32_t             writeCalls = 0;
    void (*onWrite)(const uint8_t* data, size_t len) = nullptr;

    void reset() {
        rx.clear();
        rxPos = 0;
        tx.clear();
        writeCalls = 0;
        onWrite = nullptr;
    }
    void begin(unsigned long) {}
    void flush() {}
    int available() const {
        return rxPos < rx.size() ? (int)(rx.size() - rxPos) : 0;
    }
    int read() {
        return rxPos < rx.size() ? (int)rx[rxPos++] : -1;
    }
    size_t write(const uint8_t* buf, size_t len) {
        tx.insert(tx.end(), buf, buf + len);
        writeCalls++;
        if (onWrite) onWrite(buf, len);
        return len;
    }
};

inline FakeSerial& fakeSerialInstance() {
    static FakeSerial s;
    return s;
}
#define Serial (fakeSerialInstance())

#endif
