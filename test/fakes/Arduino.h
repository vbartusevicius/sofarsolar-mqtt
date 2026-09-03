#ifndef FAKE_ARDUINO_H
#define FAKE_ARDUINO_H

// Minimal Arduino.h fake for native unit tests (only what src/ headers need
// transitively). Extend as needed.
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cstdio>

typedef bool boolean;
typedef uint8_t byte;

#ifndef PROGMEM
#define PROGMEM
#endif

#endif
