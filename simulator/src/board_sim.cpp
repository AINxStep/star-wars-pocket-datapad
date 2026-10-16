#ifdef BOARD_SIM

#include "board.h"
#include <emscripten.h>

// =========================================================================
// Web simulator board: the "panel" is an RGBA framebuffer that the page
// copies to a <canvas>; the "touch controller" is the mouse/finger. The
// resolution is chosen by the page (240x320, the ES3C28P layout) before the
// firmware's setup() runs.
// =========================================================================
static int sim_width = 240, sim_height = 320;
static uint32_t* fb = nullptr;           // RGBA, as <canvas> ImageData expects
static uint32_t frames = 0;              // bumped on every flushed area
static bool ptr_down = false;
static bool ptr_latch = false;           // a press seen since the last read, even if already released
static int ptr_x = 0, ptr_y = 0;
static SPIClass no_sd;

extern "C" {
EMSCRIPTEN_KEEPALIVE void sim_configure(int w, int h) { sim_width = w; sim_height = h; }
EMSCRIPTEN_KEEPALIVE uint32_t* sim_fb(void) { return fb; }
EMSCRIPTEN_KEEPALIVE int sim_w(void) { return sim_width; }
EMSCRIPTEN_KEEPALIVE int sim_h(void) { return sim_height; }
EMSCRIPTEN_KEEPALIVE uint32_t sim_frames(void) { return frames; }
EMSCRIPTEN_KEEPALIVE void sim_pointer(int x, int y, int down) {
    ptr_x = x; ptr_y = y; ptr_down = down != 0;
    if (down) ptr_latch = true;
}
}

EM_JS(void, js_set_brightness, (int pct), { if (window.simBrightness) window.simBrightness(pct); });
EM_JS(void, js_set_led, (int r, int g, int b), { if (window.simLed) window.simLed(r, g, b); });

static lv_disp_draw_buf_t draw_buf;
static lv_color_t* lines = nullptr;
static const int BUF_LINES = 40;

static volatile bool strobe_hold = false;   // beacon strobe: drop LVGL flushes

static void disp_flush(lv_disp_drv_t* d, const lv_area_t* a, lv_color_t* c) {
    if (strobe_hold) { lv_disp_flush_ready(d); return; }
    for (int y = a->y1; y <= a->y2; y++) {
        uint32_t* dst = &fb[y * sim_width + a->x1];
        for (int x = a->x1; x <= a->x2; x++, c++, dst++) {
            uint16_t p = c->full;                          // RGB565
            uint32_t r = (p >> 11) & 31, g = (p >> 5) & 63, b = p & 31;
            r = (r * 527 + 23) >> 6;                       // expand to 8 bits
            g = (g * 259 + 33) >> 6;
            b = (b * 527 + 23) >> 6;
            *dst = 0xFF000000u | (b << 16) | (g << 8) | r;
        }
    }
    frames++;
    lv_disp_flush_ready(d);
}

void board_init_io() {}
SPIClass& board_sd_spi() { return no_sd; }

void board_init_display() {
    fb = (uint32_t*)calloc((size_t)sim_width * sim_height, sizeof(uint32_t));
    lines = (lv_color_t*)malloc((size_t)sim_width * BUF_LINES * sizeof(lv_color_t));
    lv_disp_draw_buf_init(&draw_buf, lines, NULL, sim_width * BUF_LINES);

    static lv_disp_drv_t drv;
    lv_disp_drv_init(&drv);
    drv.hor_res = sim_width;
    drv.ver_res = sim_height;
    drv.flush_cb = disp_flush;
    drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&drv);
}

void board_set_backlight(uint8_t pct) { js_set_brightness(pct); }
int board_ldr_raw() { return -1; }
int board_batt_pct() { return -1; }
void board_set_rgb(bool r, bool g, bool b) { js_set_led(r, g, b); }
bool board_boot_pressed() { return false; }
void board_dim_shadow(bool) {}
void board_storage_begin() {}
void board_storage_end() {}
// Panel power: main.cpp calls this unconditionally for the sleep timer.
// The sim keeps drawing anyway (the host page dims via backlight), so it is
// a no-op that only matters for link completeness.
void board_display_power(bool) {}
void board_enter_deep_sleep(void) {}

// Beacon strobe: one flat fill of the whole canvas framebuffer
void board_strobe_begin(void) { strobe_hold = true; }
void board_strobe_fill(uint16_t c) {
    uint32_t r = (c >> 11) & 31, g = (c >> 5) & 63, b = c & 31;
    r = (r * 527 + 23) >> 6;
    g = (g * 259 + 33) >> 6;
    b = (b * 527 + 23) >> 6;
    uint32_t px = 0xFF000000u | (b << 16) | (g << 8) | r;
    for (int i = 0; i < sim_width * sim_height; i++) fb[i] = px;
    frames++;
}
void board_strobe_end(void) { strobe_hold = false; }

bool board_touch_read(int16_t* sx, int16_t* sy, int16_t* raw_x, int16_t* raw_y) {
    // A tap shorter than one loop iteration still has to be seen as a press
    if (!ptr_down && !ptr_latch) return false;
    ptr_latch = false;
    *sx = *raw_x = (int16_t)constrain(ptr_x, 0, sim_width - 1);
    *sy = *raw_y = (int16_t)constrain(ptr_y, 0, sim_height - 1);
    return true;
}

#endif  // BOARD_SIM
