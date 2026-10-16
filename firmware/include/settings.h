#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include "board.h"

// =========================================================================
// Persistent user settings (stored in NVS under namespace "datapad")
// =========================================================================

struct Settings {
    uint8_t brightness = 100;   // 0 = AUTO (LDR), 10..100 % manual
    uint8_t volume     = 60;    // 0..100 %
    uint8_t language   = 0;     // Lang (i18n.h): 0 Spanish, 1 English, 2 Aurebesh
    uint8_t sleep_en   = 1;     // power saving: screen off at 3 min idle, deep sleep at 60 min (ES3C28P)
    uint8_t batt_en    = 0;     // real battery % (needs the GPIO 35 divider mod)
    uint8_t theme      = 0;     // 0 Tatooine (default), 1 Imperial

    // Touch calibration: raw XPT2046 range mapped to the 240x320 screen
    int16_t touch_x_min = 300;
    int16_t touch_x_max = 3800;
    int16_t touch_y_min = 180;
    int16_t touch_y_max = 3850;
};

inline Settings& settings() {
    static Settings s;
    return s;
}

inline void settings_load() {
    Settings& s = settings();
    Preferences p;
    p.begin("datapad", false);
    s.brightness  = constrain(p.getUChar("bright", s.brightness), 0, 100);
    s.volume      = constrain(p.getUChar("volume", s.volume), 0, 100);
    s.language    = constrain(p.getUChar("lang", s.language), 0, 2);
    s.sleep_en    = p.getUChar("sleep", s.sleep_en) ? 1 : 0;
    s.batt_en     = p.getUChar("batten", s.batt_en) ? 1 : 0;
    s.theme       = constrain(p.getUChar("theme", s.theme), 0, 1);
    s.touch_x_min = p.getShort("tx_min", s.touch_x_min);
    s.touch_x_max = p.getShort("tx_max", s.touch_x_max);
    s.touch_y_min = p.getShort("ty_min", s.touch_y_min);
    s.touch_y_max = p.getShort("ty_max", s.touch_y_max);
    p.end();
}

inline void settings_save() {
    const Settings& s = settings();
    board_storage_begin();   // some boards must quiet the display before flash writes
    Preferences p;
    p.begin("datapad", false);
    p.putUChar("bright", s.brightness);
    p.putUChar("volume", s.volume);
    p.putUChar("lang", s.language);
    p.putUChar("sleep", s.sleep_en);
    p.putUChar("batten", s.batt_en);
    p.putUChar("theme", s.theme);
    p.putShort("tx_min", s.touch_x_min);
    p.putShort("tx_max", s.touch_x_max);
    p.putShort("ty_min", s.touch_y_min);
    p.putShort("ty_max", s.touch_y_max);
    p.end();
    board_storage_end();
}

// Saves ~1.5 s after the last call: a slider drag ends in a burst of changes
// and every save costs a flash write (and, on the S3, a panel restart)
inline void settings_save_soon() {
    static lv_timer_t* t = NULL;
    if (t) { lv_timer_reset(t); return; }
    t = lv_timer_create([](lv_timer_t*) { t = NULL; settings_save(); }, 1500, NULL);
    lv_timer_set_repeat_count(t, 1);
}

// Latest raw touch sample, published by the LVGL touch callback so the
// calibration screen can read it. `seq` increments on every new sample.
struct TouchRaw {
    int      x = 0, y = 0;
    bool     pressed = false;
    uint32_t seq = 0;
};

inline TouchRaw& touch_raw() {
    static TouchRaw t;
    return t;
}

// Implemented in main.cpp: applies a 10..100 % backlight level
void app_apply_brightness(uint8_t pct);

// Implemented in main.cpp: >=0 forces that backlight level (suspends AUTO
// and display sleep); -1 releases and reapplies the user's setting
void app_brightness_override(int pct);
