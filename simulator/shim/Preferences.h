#pragma once
#include <stdint.h>

// NVS stand-in backed by the browser's localStorage (see src/sim_support.cpp)
extern "C" {
int sim_pref_get(const char* ns, const char* key, int def);
void sim_pref_put(const char* ns, const char* key, int value);
}

class Preferences {
    const char* ns_ = "";
public:
    bool begin(const char* name, bool = false) { ns_ = name; return true; }
    void end() {}
    uint8_t getUChar(const char* k, uint8_t def = 0) { return (uint8_t)sim_pref_get(ns_, k, def); }
    int16_t getShort(const char* k, int16_t def = 0) { return (int16_t)sim_pref_get(ns_, k, def); }
    size_t putUChar(const char* k, uint8_t v) { sim_pref_put(ns_, k, v); return 1; }
    size_t putShort(const char* k, int16_t v) { sim_pref_put(ns_, k, v); return 2; }
};
