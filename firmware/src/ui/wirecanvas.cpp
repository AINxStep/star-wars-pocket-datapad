#include "wirecanvas.h"
#include <math.h>
#include <stdlib.h>
#include <esp_attr.h>

#define ELL_SEGS 72

// Source-over blend of one RGB565 pixel; opa 0..255
static inline IRAM_ATTR uint16_t blend565(uint16_t d, uint16_t s, uint8_t opa) {
    uint32_t a = (opa + 4) >> 3;                      // 0..32
    uint32_t s32 = (s | ((uint32_t)s << 16)) & 0x07E0F81F;
    uint32_t d32 = (d | ((uint32_t)d << 16)) & 0x07E0F81F;
    uint32_t r = (s32 * a + d32 * (32 - a)) >> 5;
    r &= 0x07E0F81F;
    return (uint16_t)(r | (r >> 16));
}

WireTarget wt_begin(const lv_draw_ctx_t* ctx) {
    WireTarget t;
    t.base = (uint16_t*)ctx->buf;
    t.stride = lv_area_get_width(ctx->buf_area);
    t.bx = ctx->buf_area->x1;
    t.by = ctx->buf_area->y1;
    t.cx1 = ctx->clip_area->x1;
    t.cy1 = ctx->clip_area->y1;
    t.cx2 = ctx->clip_area->x2;
    t.cy2 = ctx->clip_area->y2;
    return t;
}

static inline IRAM_ATTR void put(const WireTarget& t, int x, int y, uint16_t col, uint8_t opa) {
    if (x < t.cx1 || x > t.cx2 || y < t.cy1 || y > t.cy2) return;
    uint16_t* p = &t.base[(y - t.by) * t.stride + (x - t.bx)];
    *p = opa == 255 ? col : blend565(*p, col, opa);
}

IRAM_ATTR void wt_fill_rect(const WireTarget& t, int x1, int y1, int x2, int y2, lv_color_t col, uint8_t opa) {
    if (x1 < t.cx1) x1 = t.cx1;
    if (x2 > t.cx2) x2 = t.cx2;
    if (y1 < t.cy1) y1 = t.cy1;
    if (y2 > t.cy2) y2 = t.cy2;
    if (x1 > x2) return;
    const uint16_t v = col.full;
    const uint32_t v2 = v | ((uint32_t)v << 16);
    for (int y = y1; y <= y2; y++) {
        uint16_t* p = &t.base[(y - t.by) * t.stride - t.bx];
        if (opa == 255) {
            int x = x1;
            if (((x - t.bx) & 1) && x <= x2) p[x++] = v;                       // align to 32 bits
            for (; x + 1 <= x2; x += 2) *(uint32_t*)&p[x] = v2;       // two pixels per store
            if (x <= x2) p[x] = v;
        } else {
            for (int x = x1; x <= x2; x++) p[x] = blend565(p[x], v, opa);
        }
    }
}

IRAM_ATTR void wt_hline(const WireTarget& t, int y, int x0, int x1, lv_color_t col, uint8_t opa) {
    wt_fill_rect(t, x0, y, x1, y, col, opa);
}

// Pen footprint of a line `width` pixels thick, stamped at (x, y). When the
// whole footprint lies inside the clip rectangle (almost always) the per-pixel
// bounds checks are skipped.
static inline IRAM_ATTR void stamp(const WireTarget& t, int x, int y, uint16_t v, uint8_t opa, int lo, int hi) {
    if (lo == 0 && hi == 0) { put(t, x, y, v, opa); return; }
    if (x + lo >= t.cx1 && x + hi <= t.cx2 && y + lo >= t.cy1 && y + hi <= t.cy2) {
        for (int oy = lo; oy <= hi; oy++) {
            uint16_t* p = &t.base[(y + oy - t.by) * t.stride + (x + lo - t.bx)];
            if (opa == 255) { for (int ox = lo; ox <= hi; ox++, p++) *p = v; }
            else            { for (int ox = lo; ox <= hi; ox++, p++) *p = blend565(*p, v, opa); }
        }
        return;
    }
    for (int oy = lo; oy <= hi; oy++)
        for (int ox = lo; ox <= hi; ox++) put(t, x + ox, y + oy, v, opa);
}

// DDA along the major axis (16.16 fixed point). Because the minor coordinate
// is a pure function of the major one, a render band can start in the middle
// of the line: long diagonals cost only the part that falls inside the band.
IRAM_ATTR void wt_line(const WireTarget& t, int x0, int y0, int x1, int y1, lv_color_t col, uint8_t opa, int width) {
    if (opa == 0) return;
    const uint16_t v = col.full;
    const int lo = -(width - 1) / 2, hi = width / 2;   // pen footprint

    // Bounding-box reject first: with ~15 render bands per frame most
    // segments miss the band being drawn, and must cost next to nothing
    {
        int lo_x = x0 < x1 ? x0 : x1, hi_x = x0 < x1 ? x1 : x0;
        int lo_y = y0 < y1 ? y0 : y1, hi_y = y0 < y1 ? y1 : y0;
        if (hi_x + hi < t.cx1 || lo_x + lo > t.cx2 || hi_y + hi < t.cy1 || lo_y + lo > t.cy2) return;
    }
    int dx = x1 - x0, dy = y1 - y0;

    if (abs(dy) >= abs(dx)) {                          // y-major
        if (dy < 0) { int tx = x0; x0 = x1; x1 = tx; int ty = y0; y0 = y1; y1 = ty; dx = -dx; dy = -dy; }
        int ys = y0 > t.cy1 - hi ? y0 : t.cy1 - hi;
        int ye = y1 < t.cy2 - lo ? y1 : t.cy2 - lo;
        if (ys > ye) return;
        if (dy == 0) { stamp(t, x0, y0, v, opa, lo, hi); return; }
        const int slope = (int)(((int64_t)dx << 16) / dy);
        int xf = (x0 << 16) + (ys - y0) * slope + 0x8000;
        for (int y = ys; y <= ye; y++, xf += slope) {
            int x = xf >> 16;
            if (x >= t.cx1 - hi && x <= t.cx2 - lo) stamp(t, x, y, v, opa, lo, hi);
        }
    } else {                                           // x-major
        if (dx < 0) { int tx = x0; x0 = x1; x1 = tx; int ty = y0; y0 = y1; y1 = ty; dx = -dx; dy = -dy; }
        int xs = x0 > t.cx1 - hi ? x0 : t.cx1 - hi;
        int xe = x1 < t.cx2 - lo ? x1 : t.cx2 - lo;
        if (xs > xe) return;
        const int slope = (int)(((int64_t)dy << 16) / dx);
        int yf = (y0 << 16) + (xs - x0) * slope + 0x8000;
        for (int x = xs; x <= xe; x++, yf += slope) {
            int y = yf >> 16;
            if (y >= t.cy1 - hi && y <= t.cy2 - lo) stamp(t, x, y, v, opa, lo, hi);
        }
    }
}

void wt_ellipse(const WireTarget& t, int cx, int cy, int rx, int ry, lv_color_t col, uint8_t opa, bool fill) {
    if (rx < 1 || ry < 1) return;
    if (cx + rx < t.cx1 || cx - rx > t.cx2 || cy + ry < t.cy1 || cy - ry > t.cy2) return;
    if (fill) {
        int y0 = cy - ry, y1 = cy + ry;
        if (y0 < t.cy1) y0 = t.cy1;
        if (y1 > t.cy2) y1 = t.cy2;
        for (int y = y0; y <= y1; y++) {
            float k = 1.0f - (float)((y - cy) * (y - cy)) / (float)(ry * ry);
            int hw = (int)(rx * sqrtf(k > 0 ? k : 0) + 0.5f);
            wt_hline(t, y, cx - hw, cx + hw, col, opa);
        }
    } else {
        // Polyline over a precomputed unit circle (one trig table for every ring)
        static float ct[ELL_SEGS + 1], st[ELL_SEGS + 1];
        static bool ready = false;
        if (!ready) {
            for (int i = 0; i <= ELL_SEGS; i++) {
                ct[i] = cosf(6.2831853f * i / ELL_SEGS);
                st[i] = sinf(6.2831853f * i / ELL_SEGS);
            }
            ready = true;
        }
        int px_ = cx + rx, py_ = cy;
        for (int i = 1; i <= ELL_SEGS; i++) {
            int x = cx + (int)(rx * ct[i] + (ct[i] >= 0 ? 0.5f : -0.5f));
            int y = cy + (int)(ry * st[i] + (st[i] >= 0 ? 0.5f : -0.5f));
            wt_line(t, px_, py_, x, y, col, opa, 1);
            px_ = x; py_ = y;
        }
    }
}
