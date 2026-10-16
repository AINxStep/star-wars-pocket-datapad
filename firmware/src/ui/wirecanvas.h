#pragma once
#include <lvgl.h>

// =========================================================================
// WireTarget: a tiny software rasterizer for the vector-style screens
// (hologram projector, radar) on the large 800x480 layout.
//
// LVGL's own line renderer goes through anti-aliasing masks, which is far too
// slow for hundreds of translucent diagonal lines per frame. Instead, the
// custom draw callbacks rasterize straight into LVGL's current render buffer
// (a band of the screen in fast internal RAM), using Bresenham lines and a
// packed RGB565 alpha blend. Coordinates are screen coordinates; everything
// is clipped to the band being rendered.
// =========================================================================
struct WireTarget {
    uint16_t* base;             // pixel buffer of the band (native RGB565, LV_COLOR_16_SWAP = 0)
    int stride;                 // pixels per buffer row
    int bx, by;                 // screen coordinates of buffer pixel (0,0)
    int cx1, cy1, cx2, cy2;     // clip rectangle (inclusive), screen coordinates
};

// Target for the render band LVGL is currently drawing (valid inside a draw event)
WireTarget wt_begin(const lv_draw_ctx_t* ctx);

void wt_fill_rect(const WireTarget& t, int x1, int y1, int x2, int y2, lv_color_t col, uint8_t opa);
void wt_line(const WireTarget& t, int x0, int y0, int x1, int y1, lv_color_t col, uint8_t opa, int width = 1);
void wt_hline(const WireTarget& t, int y, int x0, int x1, lv_color_t col, uint8_t opa);   // inclusive span
void wt_ellipse(const WireTarget& t, int cx, int cy, int rx, int ry, lv_color_t col, uint8_t opa, bool fill);
inline void wt_disc(const WireTarget& t, int cx, int cy, int r, lv_color_t col, uint8_t opa) {
    wt_ellipse(t, cx, cy, r, r, col, opa, true);
}
