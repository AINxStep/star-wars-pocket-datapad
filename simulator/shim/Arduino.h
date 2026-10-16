#pragma once
// Minimal Arduino core stand-in so the firmware's portable code (UI, i18n,
// settings, main loop) builds unchanged for the web simulator. LVGL's C
// sources include this header too (LV_TICK_CUSTOM), so the C++ part is guarded.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif
uint32_t millis(void);
uint32_t micros(void);
#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
#include <algorithm>
using std::min;
using std::max;

inline void delay(uint32_t) {}        // the browser loop must never block

inline long random(long max) { return max > 0 ? rand() % max : 0; }
inline long random(long min, long max) { return min + random(max - min); }

#ifndef constrain
#define constrain(x, a, b) ((x) < (a) ? (a) : ((x) > (b) ? (b) : (x)))
#endif
inline long map(long x, long in_min, long in_max, long out_min, long out_max) {
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

class SerialClass {
public:
    void begin(unsigned long) {}
    void println(const char* s);
    void print(const char* s);
    int printf(const char* fmt, ...) __attribute__((format(printf, 2, 3)));
    // TinyUSB "host attached" check (main.cpp gates periodic prints on it):
    // in the browser the log panel is always there, so always connected.
    operator bool() const { return true; }
};
extern SerialClass Serial;

// Free-heap queries (printed once at boot): the browser has no meaningful numbers
class EspClass {
public:
    uint32_t getFreeHeap() { return 0; }
    uint32_t getMaxAllocHeap() { return 0; }
    void restart();                 // implemented via location.reload()
};
extern EspClass ESP;
#endif
