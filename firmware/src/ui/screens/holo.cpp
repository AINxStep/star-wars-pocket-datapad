#include "../ui_common.h"
#include "holo_models.h"
#include "../wirecanvas.h"
#include <math.h>
#include <stdio.h>

// =========================================================================
// Hologram projector: live wireframe models with a holographic look
// (depth-faded cyan lines, flicker, glitches, scanlines, projector base).
// Drag to rotate; the arrows or a lateral flick switch model.
// =========================================================================

// Viewport geometry: 240x214 on the portrait layout, the left part of the
// screen on the wide one. Everything is derived from the height through K.
static int VW = 240, VH = 214;    // viewport size
static int MODEL_CX = 120, MODEL_CY = 96;
static float MODEL_PX = 80.0f;    // on-screen radius of every model, in px
static int BASE_Y = 188;          // projector base
static float K = 1.0f;            // scale relative to the portrait design
#define KI(v) ((int)((v) * K + 0.5f))
#define APPEAR_MS 700
#define DEG (3.14159265f / 180.0f)

static lv_obj_t* scr_holo;
static lv_obj_t* viewport;
static lv_obj_t* lbl_name;
static lv_obj_t* lbl_desc;
static lv_obj_t* lbl_count;

static lv_timer_t* holo_timer;
static HoloModel model;
static int cur = 0;

static float yaw = 20, pitch = 14;
static const float spin_dps = 26;
static bool pressing = false;
static uint32_t last_touch = 0, last_tick = 0, appear_start = 0;
static uint32_t glitch_until = 0, next_glitch = 0;
static int glitch_y = 0, glitch_dx = 0;
static bool was_active = false;

// Projected geometry (viewport coordinates), refreshed by the timer
static float px[HOLO_MAXV], py[HOLO_MAXV], pz[HOLO_MAXV];
static uint8_t eop[HOLO_MAXE];
static int scan_off = 0, sweep_y = 0;

static lv_color_t holo_color(void) { return COLOR_HOLO; }

static void select_model(int idx, bool feedback) {
    cur = (idx % HOLO_COUNT + HOLO_COUNT) % HOLO_COUNT;
    holo_model_build(cur, &model);
    appear_start = lv_tick_get();
    lv_label_set_text(lbl_name, holo_model_name(cur));
    lv_label_set_text(lbl_desc, holo_model_desc(cur));
    lv_label_set_text_fmt(lbl_count, "%d/%d", cur + 1, HOLO_COUNT);
    if (feedback) hw_led_pulse(false, true, true, sfx_holo());
}

// -------------------------------------------------------------------------
// Per-frame update: rotate, project and compute line opacities
// -------------------------------------------------------------------------
static void holo_timer_cb(lv_timer_t* t) {
    uint32_t now = lv_tick_get();
    if (lv_scr_act() != scr_holo) {
        was_active = false;
        last_tick = now;
        return;
    }
    if (!was_active) {                       // just entered: power up the projector
        was_active = true;
        appear_start = now;
        next_glitch = now + 2500;
        hw_led_pulse(false, true, true, sfx_holo());
    }

    float dt = (now - last_tick) / 1000.0f;
    last_tick = now;
    if (dt > 0.1f) dt = 0.1f;
    if (!pressing && now - last_touch > 1500) yaw = fmodf(yaw + spin_dps * dt, 360.0f);

    // Glitch: a short burst with a displaced horizontal band and a brightness dip
    bool glitch = now < glitch_until;
    if (!glitch && (int32_t)(now - next_glitch) >= 0) {
        glitch_until = now + 90 + lv_rand(0, 120);
        glitch_y = lv_rand(KI(30), VH - KI(40));
        glitch_dx = lv_rand(0, 1) ? (int)lv_rand(KI(4), KI(9)) : -(int)lv_rand(KI(4), KI(9));
        next_glitch = glitch_until + lv_rand(2000, 5000);
        glitch = true;
    }
    int flicker = 215 + lv_rand(0, 40);
    if (glitch) flicker = flicker * 55 / 100;

    float appear = (now - appear_start) / (float)APPEAR_MS;
    if (appear > 1) appear = 1;
    float cut = BASE_Y - appear * (BASE_Y + KI(20));     // edges above this line are not yet projected

    float cy_ = cosf(yaw * DEG), sy_ = sinf(yaw * DEG), cp = cosf(pitch * DEG), sp = sinf(pitch * DEG);
    float R = model.radius, S = MODEL_PX / R, D = R * 3.0f;
    for (int i = 0; i < model.nv; i++) {
        float x = model.v[i][0], y = model.v[i][1], z = model.v[i][2];
        float x1 = x * cy_ + z * sy_, z1 = -x * sy_ + z * cy_;       // yaw
        float y2 = y * cp - z1 * sp, z2 = y * sp + z1 * cp;           // pitch
        float k = D / (D - z2);                                       // perspective
        px[i] = MODEL_CX + x1 * k * S;
        py[i] = MODEL_CY - y2 * k * S;
        pz[i] = z2 / R;
        if (glitch && fabsf(py[i] - glitch_y) < KI(14)) px[i] += glitch_dx;
    }
    for (int i = 0; i < model.ne; i++) {
        int a = model.e[i][0], b = model.e[i][1];
        float ymin = py[a] < py[b] ? py[a] : py[b];
        if (ymin < cut) { eop[i] = 0; continue; }
        float depth = ((pz[a] + pz[b]) * 0.5f + 1.0f) * 0.5f;       // 1 = near
        int o = (int)((55 + 200 * depth) * flicker / 255);
        eop[i] = o > 255 ? 255 : (o < 0 ? 0 : o);
    }

    scan_off = (now / 60) % 5;
    sweep_y = VH - (int)((now / 10) % (VH + KI(60))) + KI(30);
    lv_obj_invalidate(viewport);
}

// -------------------------------------------------------------------------
// Drawing
// -------------------------------------------------------------------------
static void holo_draw_cb(lv_event_t* e) {
    lv_obj_t* obj = lv_event_get_target(e);
    lv_draw_ctx_t* ctx = lv_event_get_draw_ctx(e);
    lv_area_t a;
    lv_obj_get_coords(obj, &a);
    lv_coord_t ox = a.x1, oy = a.y1;
    lv_color_t col = holo_color();

    // Projection chamber
    lv_draw_rect_dsc_t bg;
    lv_draw_rect_dsc_init(&bg);
    bg.bg_color = COLOR_CHAMBER;
    bg.bg_opa = LV_OPA_COVER;
    lv_draw_rect(ctx, &bg, &a);

    // Light cone from the base
    lv_draw_line_dsc_t ld;
    lv_draw_line_dsc_init(&ld);
    ld.color = col;
    ld.width = 1;
    ld.opa = LV_OPA_20;
    lv_point_t p1 = {(lv_coord_t)(ox + MODEL_CX - KI(92)), (lv_coord_t)(oy + BASE_Y)};
    lv_point_t p2 = {(lv_coord_t)(ox + MODEL_CX - KI(118)), oy};
    lv_draw_line(ctx, &ld, &p1, &p2);
    p1.x = ox + MODEL_CX + KI(92);
    p2.x = ox + MODEL_CX + KI(118);
    lv_draw_line(ctx, &ld, &p1, &p2);

    // Projector platform: three concentric flattened rings and the emitter
    static const int rx[3] = {92, 66, 40};
    static const int ry[3] = {22, 16, 10};
    static const lv_opa_t ro[3] = {90, 65, 45};
    for (int i = 0; i < 3; i++) {
        lv_draw_rect_dsc_t ring;
        lv_draw_rect_dsc_init(&ring);
        ring.bg_color = col;
        ring.bg_opa = i == 0 ? LV_OPA_10 : LV_OPA_TRANSP;
        ring.border_color = col;
        ring.border_width = 1;
        ring.border_opa = ro[i];
        ring.radius = LV_RADIUS_CIRCLE;
        lv_area_t ra = {(lv_coord_t)(ox + MODEL_CX - KI(rx[i])), (lv_coord_t)(oy + BASE_Y - KI(ry[i])),
                        (lv_coord_t)(ox + MODEL_CX + KI(rx[i])), (lv_coord_t)(oy + BASE_Y + KI(ry[i]))};
        lv_draw_rect(ctx, &ring, &ra);
    }
    lv_draw_rect_dsc_t em;
    lv_draw_rect_dsc_init(&em);
    em.bg_color = COLOR_HILITE;
    em.bg_opa = LV_OPA_80;
    em.radius = 2;
    lv_area_t ea = {(lv_coord_t)(ox + MODEL_CX - KI(9)), (lv_coord_t)(oy + BASE_Y - KI(3)),
                    (lv_coord_t)(ox + MODEL_CX + KI(9)), (lv_coord_t)(oy + BASE_Y + KI(2))};
    lv_draw_rect(ctx, &em, &ea);

    // Model edges, brighter when nearer the viewer
    for (int i = 0; i < model.ne; i++) {
        if (eop[i] == 0) continue;
        int a_ = model.e[i][0], b_ = model.e[i][1];
        ld.color = col;
        ld.width = model.w[i] + (K > 1.5f ? 1 : 0);
        ld.opa = eop[i];
        p1 = {(lv_coord_t)(ox + (int)px[a_]), (lv_coord_t)(oy + (int)py[a_])};
        p2 = {(lv_coord_t)(ox + (int)px[b_]), (lv_coord_t)(oy + (int)py[b_])};
        if (ui_line_clipped(ctx, p1.x, p1.y, p2.x, p2.y, 3)) continue;
        lv_draw_line(ctx, &ld, &p1, &p2);
    }

    // Scanlines and the rising scan sweep, on top of everything
    ld.color = col;
    ld.width = 1;
    ld.opa = LV_OPA_10;
    for (int y = scan_off; y < VH; y += KI(5)) {
        p1 = {ox, (lv_coord_t)(oy + y)};
        p2 = {(lv_coord_t)(ox + VW), (lv_coord_t)(oy + y)};
        if (ui_line_clipped(ctx, p1.x, p1.y, p2.x, p2.y, 1)) continue;
        lv_draw_line(ctx, &ld, &p1, &p2);
    }
    if (sweep_y > 0 && sweep_y < VH) {
        ld.width = 2;
        ld.opa = LV_OPA_30;
        p1 = {ox, (lv_coord_t)(oy + sweep_y)};
        p2 = {(lv_coord_t)(ox + VW), (lv_coord_t)(oy + sweep_y)};
        lv_draw_line(ctx, &ld, &p1, &p2);
    }
}

// Wide layout: the same scene as holo_draw_cb, but rasterized by hand straight
// into LVGL's render band (see wirecanvas.h): LVGL's own diagonal-line drawing
// is far too slow at this size.
static void holo_draw_cb_wide(lv_event_t* e) {
    lv_obj_t* obj = lv_event_get_target(e);
    lv_draw_ctx_t* ctx = lv_event_get_draw_ctx(e);
    lv_area_t a;
    lv_obj_get_coords(obj, &a);
    const int ox = a.x1, oy = a.y1;
    const WireTarget t = wt_begin(ctx);
    lv_color_t col = holo_color();

    // Projection chamber
    wt_fill_rect(t, a.x1, a.y1, a.x2, a.y2, COLOR_CHAMBER, 255);

    // Light cone from the base
    wt_line(t, ox + MODEL_CX - KI(92), oy + BASE_Y, ox + MODEL_CX - KI(118), oy, col, LV_OPA_20, 1);
    wt_line(t, ox + MODEL_CX + KI(92), oy + BASE_Y, ox + MODEL_CX + KI(118), oy, col, LV_OPA_20, 1);

    // Projector platform: three concentric flattened rings and the emitter
    static const int rx[3] = {92, 66, 40};
    static const int ry[3] = {22, 16, 10};
    static const lv_opa_t ro[3] = {90, 65, 45};
    for (int i = 0; i < 3; i++) {
        if (i == 0) wt_ellipse(t, ox + MODEL_CX, oy + BASE_Y, KI(rx[i]), KI(ry[i]), col, LV_OPA_10, true);
        wt_ellipse(t, ox + MODEL_CX, oy + BASE_Y, KI(rx[i]), KI(ry[i]), col, ro[i], false);
    }
    wt_fill_rect(t, ox + MODEL_CX - KI(9), oy + BASE_Y - KI(3), ox + MODEL_CX + KI(9), oy + BASE_Y + KI(2),
                 COLOR_HILITE, LV_OPA_80);

    // Model edges, brighter when nearer the viewer
    for (int i = 0; i < model.ne; i++) {
        if (eop[i] == 0) continue;
        int a_ = model.e[i][0], b_ = model.e[i][1];
        wt_line(t, ox + (int)px[a_], oy + (int)py[a_], ox + (int)px[b_], oy + (int)py[b_], col, eop[i],
                model.w[i]);
    }

    // Scanlines and the rising scan sweep, on top of everything
    for (int y = scan_off; y < VH; y += KI(5)) wt_hline(t, oy + y, ox, ox + VW - 1, col, LV_OPA_10);
    if (sweep_y > 0 && sweep_y < VH) {
        wt_hline(t, oy + sweep_y, ox, ox + VW - 1, col, LV_OPA_30);
        wt_hline(t, oy + sweep_y + 1, ox, ox + VW - 1, col, LV_OPA_30);
    }
}

// -------------------------------------------------------------------------
// Input
// -------------------------------------------------------------------------
static void viewport_event_cb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_PRESSING) {
        lv_point_t v;
        lv_indev_get_vect(lv_indev_get_act(), &v);
        yaw += v.x * 0.9f;
        pitch += v.y * 0.9f;
        if (pitch > 75) pitch = 75;
        if (pitch < -75) pitch = -75;
        pressing = true;
        last_touch = lv_tick_get();
    } else if (code == LV_EVENT_GESTURE) {
        // A fast lateral flick switches model; a slow drag keeps rotating
        lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
        if (dir == LV_DIR_LEFT)  select_model(cur + 1, true);
        if (dir == LV_DIR_RIGHT) select_model(cur - 1, true);
    } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        pressing = false;
        last_touch = lv_tick_get();
    }
}

static void event_holo_step(lv_event_t* e) {
    int dir = (int)(intptr_t)lv_event_get_user_data(e);
    select_model(cur + dir, true);
}

// Called by the starchart to land the projector on a specific model
void holo_show_model(int idx) {
    select_model(idx, false);
}

lv_obj_t* screen_holo_create(void) {
    const bool wide = UI_WIDE;
    VW = wide ? UI_W * 27 / 40 : 240;                 // wide: left two thirds
    VH = wide ? UI_H - TOP_BAR_H : 214;                // portrait keeps room for name + buttons
    K = VH / 214.0f;
    MODEL_CX = VW / 2;
    MODEL_CY = (int)(96 * K);
    MODEL_PX = 80.0f * K;
    BASE_Y = VH - KI(26);

    scr_holo = lv_obj_create(NULL);
    lv_obj_add_style(scr_holo, &style_base, 0);
    lv_obj_clear_flag(scr_holo, LV_OBJ_FLAG_SCROLLABLE);
    ui_attach_top_bar(scr_holo, tr(S_HOLO_TITLE), true);

    viewport = lv_obj_create(scr_holo);
    lv_obj_remove_style_all(viewport);
    lv_obj_set_size(viewport, VW, VH);
    lv_obj_set_pos(viewport, 0, TOP_BAR_H);
    lv_obj_clear_flag(viewport, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(viewport, wide ? holo_draw_cb_wide : holo_draw_cb, LV_EVENT_DRAW_MAIN_END, NULL);
    lv_obj_add_event_cb(viewport, viewport_event_cb, LV_EVENT_ALL, NULL);

    lbl_count = lv_label_create(scr_holo);
    lv_obj_set_style_text_font(lbl_count, ui_font(12), 0);
    lv_obj_set_style_text_color(lbl_count, COLOR_DATA, 0);
    lv_obj_align(lbl_count, LV_ALIGN_TOP_RIGHT, wide ? -(UI_W - VW) - 12 : -8, TOP_BAR_H + (wide ? 12 : 6));

    // Model name and description: between the arrows (portrait) or in the
    // right-hand column (wide)
    const int col_x = VW + 16, col_w = UI_W - VW - 32;
    lbl_name = lv_label_create(scr_holo);
    lv_obj_set_width(lbl_name, wide ? col_w : 130);
    lv_obj_set_style_text_align(lbl_name, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(lbl_name, ui_font(lang_get() == LANG_AUR ? 12 : 16), 0);   // glyphs are wider
    lv_obj_set_style_text_color(lbl_name, COLOR_ACCENT, 0);
    if (wide) lv_obj_set_pos(lbl_name, col_x, TOP_BAR_H + 60);
    else      lv_obj_align(lbl_name, LV_ALIGN_TOP_MID, 0, TOP_BAR_H + VH + 6);

    lbl_desc = lv_label_create(scr_holo);
    lv_obj_set_width(lbl_desc, wide ? col_w : 124);
    lv_obj_set_style_text_align(lbl_desc, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(lbl_desc, ui_font(12), 0);
    lv_obj_set_style_text_color(lbl_desc, COLOR_DATA, 0);
    if (wide) lv_obj_set_pos(lbl_desc, col_x, TOP_BAR_H + 130);
    else      lv_obj_align(lbl_desc, LV_ALIGN_TOP_MID, 0, TOP_BAR_H + VH + 28);

    for (int i = 0; i < 2; i++) {
        lv_obj_t* btn = lv_btn_create(scr_holo);
        if (wide) {
            lv_obj_set_size(btn, (col_w - 16) / 2, 70);
            lv_obj_align(btn, LV_ALIGN_BOTTOM_RIGHT, i == 0 ? -(16 + (col_w - 16) / 2 + 16) : -16, -24);
        } else {
            lv_obj_set_size(btn, 46, 44);
            lv_obj_align(btn, i == 0 ? LV_ALIGN_BOTTOM_LEFT : LV_ALIGN_BOTTOM_RIGHT, i == 0 ? 8 : -8, -6);
        }
        ui_style_btn(btn, false);
        lv_obj_add_event_cb(btn, event_holo_step, LV_EVENT_CLICKED, (void*)(intptr_t)(i == 0 ? -1 : 1));
        lv_obj_t* l = lv_label_create(btn);
        lv_label_set_text(l, i == 0 ? LV_SYMBOL_LEFT : LV_SYMBOL_RIGHT);
        lv_obj_center(l);
        if (wide) lv_obj_set_style_text_font(l, ui_font_latin(20), 0);
    }

    select_model(cur, false);
    if (!holo_timer) holo_timer = lv_timer_create(holo_timer_cb, 40, NULL);   // survives screen rebuilds
    return scr_holo;
}
