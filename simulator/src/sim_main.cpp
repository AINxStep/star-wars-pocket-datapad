#include <emscripten.h>
#include <stdint.h>
#include <stdlib.h>

// The firmware's own main.cpp provides setup() and loop() (Arduino style).
// The page calls sim_start() once and then sim_tick() from a timer: the loop
// only needs to tick every few ms for LVGL, and browsers throttle
// requestAnimationFrame freely (hidden or embedded panes can drop it to a few
// frames per second), so painting and ticking are decoupled.
void setup();
void loop();

static uint32_t ticks = 0;

extern "C" {
EMSCRIPTEN_KEEPALIVE void sim_start(void) {
    srand((unsigned)emscripten_get_now());      // rand() is unseeded otherwise: same sequence on every page load
    setup();
}
EMSCRIPTEN_KEEPALIVE void sim_tick(void) { ticks++; loop(); }
EMSCRIPTEN_KEEPALIVE uint32_t sim_loops(void) { return ticks; }
}
