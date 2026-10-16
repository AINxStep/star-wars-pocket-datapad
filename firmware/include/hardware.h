#pragma once
#include <Arduino.h>
#include "board.h"

// Hardware control, forwarded to the active board
inline void hw_set_rgb(bool r, bool g, bool b) { board_set_rgb(r, g, b); }

// Battery percentage for the top bar (-1 when the board cannot measure it)
inline int batt_read_pct() { return board_batt_pct(); }

// =========================================================================
// Non-blocking LED pulses
// hw_led_pulse() lights the LED for `ms` milliseconds (optionally strobing
// every `strobe_ms`) and hw_led_update(), called from loop(), turns it off.
// =========================================================================
struct LedState {
    bool     r = false, g = false, b = false;
    uint32_t until = 0;
    uint16_t strobe_ms = 0;
    uint32_t next_toggle = 0;
    bool     lit = false;
    bool     active = false;
};

inline LedState& led_state() {
    static LedState s;
    return s;
}

inline void hw_led_pulse(bool r, bool g, bool b, uint32_t ms, uint16_t strobe_ms = 0) {
    LedState& s = led_state();
    s.r = r; s.g = g; s.b = b;
    s.until = millis() + ms;
    s.strobe_ms = strobe_ms;
    s.next_toggle = millis() + strobe_ms;
    s.lit = true;
    s.active = true;
    hw_set_rgb(r, g, b);
}

inline void hw_led_update() {
    LedState& s = led_state();
    if (!s.active) return;
    uint32_t now = millis();
    if ((int32_t)(now - s.until) >= 0) {
        s.active = false;
        hw_set_rgb(false, false, false);
        return;
    }
    if (s.strobe_ms && (int32_t)(now - s.next_toggle) >= 0) {
        s.lit = !s.lit;
        s.next_toggle = now + s.strobe_ms;
        hw_set_rgb(s.lit && s.r, s.lit && s.g, s.lit && s.b);
    }
}
