#include "../ui_common.h"
#include "holo_models.h"
#include <math.h>

static lv_obj_t* scr_map;
static lv_obj_t* map_view;
static lv_obj_t* lbl_sys_name;
static lv_obj_t* lbl_sys_desc;
static lv_obj_t* btn_holo;
static lv_timer_t* map_timer;
static int sel = -1;

// =========================================================================
// Star chart: a stylized 2-arm spiral galaxy. Dots are tappable systems; the
// card below shows name/region and a HOLO button when the projector has a
// model for it (Alderaan opens the Death Star on purpose).
// =========================================================================
// Map geometry: 240x176 on the portrait layout, the left part of the screen
// on the wide one (the galaxy ellipse and the markers scale with it)
static int MAP_W = 240, MAP_H = 176;
static int MAP_CX = 120, MAP_CY = 88, MAP_RX = 104, MAP_RY = 66;
static float K = 1.0f;
#define KI(v) ((int)((v) * K + 0.5f))
#define N_SYS  12

struct StarSys {
    const char* name;         // proper noun: same in every language
    float x, y;               // -1..1 inside the galaxy ellipse
    int   holo;               // HoloId or -1 when no projector model exists
    StrId desc;               // S_MAP_DESC_* (region + one-liner)
};

// Order must match the S_MAP_DESC_* entries in i18n.cpp
static const StarSys SYSTEMS[N_SYS] = {
    { "CORUSCANT",  0.00f,  0.00f, HOLO_CORUSCANT,     S_MAP_DESC_0  },
    { "ALDERAAN",   0.20f, -0.22f, HOLO_DEATHSTAR,     S_MAP_DESC_1  },
    { "NABOO",      0.34f,  0.18f, -1,                 S_MAP_DESC_2  },
    { "KASHYYYK",   0.18f,  0.38f, -1,                 S_MAP_DESC_3  },
    { "MUSTAFAR",   0.44f, -0.40f, -1,                 S_MAP_DESC_4  },
    { "YAVIN",      0.62f, -0.62f, -1,                 S_MAP_DESC_5  },
    { "TATOOINE",   0.66f,  0.50f, HOLO_TATOOINE,      S_MAP_DESC_6  },
    { "BESPIN",     0.30f,  0.72f, -1,                 S_MAP_DESC_7  },
    { "JAKKU",     -0.50f, -0.40f, -1,                 S_MAP_DESC_8  },
    { "DAGOBAH",   -0.70f,  0.30f, -1,                 S_MAP_DESC_9  },
    { "HOTH",      -0.42f,  0.62f, HOLO_HOTH,          S_MAP_DESC_10 },
    { "ENDOR",     -0.62f,  0.78f, HOLO_ENDOR,         S_MAP_DESC_11 },
};

static void sys_px(int i, int* sx, int* sy) {
    *sx = MAP_CX + (int)(SYSTEMS[i].x * MAP_RX);
    *sy = MAP_CY + (int)(SYSTEMS[i].y * MAP_RY);
}

// -------------------------------------------------------------------------
// Drawing
// -------------------------------------------------------------------------
static void map_draw_cb(lv_event_t* e) {
    lv_obj_t* obj = lv_event_get_target(e);
    lv_draw_ctx_t* ctx = lv_event_get_draw_ctx(e);
    lv_area_t a;
    lv_obj_get_coords(obj, &a);
    lv_coord_t ox = a.x1, oy = a.y1;
    uint32_t now = lv_tick_get();

    lv_draw_rect_dsc_t rc;
    lv_draw_rect_dsc_init(&rc);
    rc.bg_color = COLOR_CHAMBER;
    rc.bg_opa = LV_OPA_COVER;
    lv_draw_rect(ctx, &rc, &a);

    // Background star scatter (deterministic, no state needed)
    lv_draw_rect_dsc_t st;
    lv_draw_rect_dsc_init(&st);
    st.bg_color = COLOR_DATA;
    st.radius = LV_RADIUS_CIRCLE;
    const int n_stars = K > 1.5f ? 110 : 36;
    for (int i = 0; i < n_stars; i++) {
        int sx = (i * 67 + 13) % MAP_W, sy = (i * 41 + 29) % MAP_H;
        st.bg_opa = (lv_opa_t)(40 + (i * 53) % 60);
        lv_area_t sa = {(lv_coord_t)(ox + sx), (lv_coord_t)(oy + sy),
                        (lv_coord_t)(ox + sx + (K > 1.5f ? 2 : 1)), (lv_coord_t)(oy + sy + (K > 1.5f ? 2 : 1))};
        lv_draw_rect(ctx, &st, &sa);
    }

    // Two spiral arms sweeping out of the core
    lv_draw_line_dsc_t ln;
    lv_draw_line_dsc_init(&ln);
    ln.color = COLOR_HOLO;
    ln.width = K > 1.5f ? 2 : 1;
    for (int arm = 0; arm < 2; arm++) {
        float ph = arm * 3.14159265f;
        int lx = MAP_CX, ly = MAP_CY;
        for (int i = 1; i <= 40; i++) {
            float t = i / 40.0f;
            float ang = ph + t * 4.4f;
            float r = 0.10f + 0.92f * t;
            int px = MAP_CX + (int)(cosf(ang) * r * MAP_RX);
            int py = MAP_CY + (int)(sinf(ang) * r * MAP_RY);
            ln.opa = (lv_opa_t)(90 - 55 * t);
            lv_point_t p1 = {(lv_coord_t)(ox + lx), (lv_coord_t)(oy + ly)};
            lv_point_t p2 = {(lv_coord_t)(ox + px), (lv_coord_t)(oy + py)};
            lv_draw_line(ctx, &ln, &p1, &p2);
            lx = px; ly = py;
        }
    }

    // Galactic core glow
    lv_draw_rect_dsc_t core;
    lv_draw_rect_dsc_init(&core);
    core.bg_color = COLOR_HOLO;
    core.radius = LV_RADIUS_CIRCLE;
    core.bg_opa = LV_OPA_30;
    lv_area_t ca = {(lv_coord_t)(ox + MAP_CX - KI(12)), (lv_coord_t)(oy + MAP_CY - KI(12)),
                    (lv_coord_t)(ox + MAP_CX + KI(12)), (lv_coord_t)(oy + MAP_CY + KI(12))};
    lv_draw_rect(ctx, &core, &ca);
    core.bg_opa = LV_OPA_70;
    lv_area_t cb = {(lv_coord_t)(ox + MAP_CX - KI(5)), (lv_coord_t)(oy + MAP_CY - KI(5)),
                    (lv_coord_t)(ox + MAP_CX + KI(5)), (lv_coord_t)(oy + MAP_CY + KI(5))};
    lv_draw_rect(ctx, &core, &cb);

    // Systems
    lv_draw_rect_dsc_t dot;
    lv_draw_rect_dsc_init(&dot);
    dot.radius = LV_RADIUS_CIRCLE;
    for (int i = 0; i < N_SYS; i++) {
        int sx, sy;
        sys_px(i, &sx, &sy);
        dot.bg_color = SYSTEMS[i].holo >= 0 ? COLOR_HOLO : COLOR_DATA;
        dot.bg_opa = LV_OPA_COVER;
        dot.border_width = 0;
        int r = KI(i == sel ? 4 : 3);
        lv_area_t da = {(lv_coord_t)(ox + sx - r), (lv_coord_t)(oy + sy - r),
                        (lv_coord_t)(ox + sx + r), (lv_coord_t)(oy + sy + r)};
        lv_draw_rect(ctx, &dot, &da);

        if (i == sel) {   // pulsing selection ring
            int pr = KI(6) + (int)((now / 140) % 4) * (int)K;
            lv_draw_rect_dsc_t ring;
            lv_draw_rect_dsc_init(&ring);
            ring.bg_opa = LV_OPA_TRANSP;
            ring.border_color = COLOR_ACCENT;
            ring.border_width = 2;
            ring.border_opa = (lv_opa_t)(255 - (int)((now / 140) % 4) * 55);
            ring.radius = LV_RADIUS_CIRCLE;
            lv_area_t ra = {(lv_coord_t)(ox + sx - pr), (lv_coord_t)(oy + sy - pr),
                            (lv_coord_t)(ox + sx + pr), (lv_coord_t)(oy + sy + pr)};
            lv_draw_rect(ctx, &ring, &ra);
        }
    }
}

static void map_timer_cb(lv_timer_t* t) {
    if (lv_scr_act() != scr_map || sel < 0) return;
    lv_obj_invalidate(map_view);   // animate the selection ring
}

// -------------------------------------------------------------------------
// Selection and the info card
// -------------------------------------------------------------------------
static void map_show_sel(void) {
    if (sel < 0) {
        lv_label_set_text(lbl_sys_name, tr(S_MAP_TAP));
        lv_label_set_text(lbl_sys_desc, "");
        lv_obj_add_flag(btn_holo, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    const StarSys& s = SYSTEMS[sel];
    lv_label_set_text(lbl_sys_name, s.name);
    lv_label_set_text(lbl_sys_desc, tr(s.desc));
    if (s.holo >= 0) lv_obj_clear_flag(btn_holo, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(btn_holo, LV_OBJ_FLAG_HIDDEN);
}

static void map_touch_cb(lv_event_t* e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    lv_point_t p;
    lv_indev_get_point(lv_indev_get_act(), &p);
    lv_area_t a;
    lv_obj_get_coords(map_view, &a);
    int px = p.x - a.x1, py = p.y - a.y1;

    int best = -1, best_d = KI(20) * KI(20);
    for (int i = 0; i < N_SYS; i++) {
        int sx, sy;
        sys_px(i, &sx, &sy);
        int dx = px - sx, dy = py - sy;
        int d = dx * dx + dy * dy;
        if (d < best_d) { best_d = d; best = i; }
    }
    if (best < 0) return;
    sel = best;
    sfx_click();
    map_show_sel();
    lv_obj_invalidate(map_view);
}

static void event_holo_btn(lv_event_t* e) {
    if (sel < 0 || SYSTEMS[sel].holo < 0) return;
    holo_show_model(SYSTEMS[sel].holo);
    ui_goto_holo();
}

lv_obj_t* screen_starchart_create(void) {
    const bool wide = UI_WIDE;
    MAP_W = wide ? UI_W * 27 / 40 : 240;
    MAP_H = wide ? UI_H - TOP_BAR_H : 176;
    K = wide ? MAP_H / 176.0f * 0.8f : 1.0f;
    MAP_CX = MAP_W / 2;
    MAP_CY = MAP_H / 2;
    MAP_RX = MAP_W * 104 / 240;
    MAP_RY = MAP_H * 66 / 176;

    sel = -1;
    scr_map = lv_obj_create(NULL);
    lv_obj_add_style(scr_map, &style_base, 0);
    lv_obj_clear_flag(scr_map, LV_OBJ_FLAG_SCROLLABLE);
    ui_attach_top_bar(scr_map, tr(S_MAP_TITLE), true);

    map_view = lv_obj_create(scr_map);
    lv_obj_remove_style_all(map_view);
    lv_obj_set_size(map_view, MAP_W, MAP_H);
    lv_obj_set_pos(map_view, 0, TOP_BAR_H);
    lv_obj_clear_flag(map_view, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(map_view, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(map_view, map_draw_cb, LV_EVENT_DRAW_MAIN_END, NULL);
    lv_obj_add_event_cb(map_view, map_touch_cb, LV_EVENT_ALL, NULL);

    // Info card: name, region line and (when available) a HOLO shortcut.
    // Portrait: strip under the map. Wide: column to the right of the map.
    lv_obj_t* card = lv_obj_create(scr_map);
    if (wide) {
        lv_obj_set_size(card, UI_W - MAP_W - 32, UI_H - TOP_BAR_H - 32);
        lv_obj_set_pos(card, MAP_W + 16, TOP_BAR_H + 16);
    } else {
        lv_obj_set_size(card, 216, 84);
        lv_obj_align(card, LV_ALIGN_BOTTOM_MID, 0, -8);
    }
    lv_obj_add_style(card, &style_card, 0);
    lv_obj_set_style_pad_all(card, wide ? 14 : 6, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    lbl_sys_name = lv_label_create(card);
    lv_obj_set_style_text_font(lbl_sys_name, ui_font(wide ? 20 : 16), 0);
    lv_obj_set_style_text_color(lbl_sys_name, COLOR_ACCENT, 0);
    lv_obj_set_width(lbl_sys_name, wide ? UI_W - MAP_W - 32 - 28 : LV_SIZE_CONTENT);
    lv_label_set_long_mode(lbl_sys_name, LV_LABEL_LONG_WRAP);
    lv_obj_align(lbl_sys_name, LV_ALIGN_TOP_LEFT, 4, 2);

    lbl_sys_desc = lv_label_create(card);
    lv_obj_set_width(lbl_sys_desc, wide ? UI_W - MAP_W - 32 - 28 : 130);
    lv_label_set_long_mode(lbl_sys_desc, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(lbl_sys_desc, ui_font(12), 0);
    lv_obj_set_style_text_color(lbl_sys_desc, COLOR_DATA, 0);
    lv_obj_align(lbl_sys_desc, LV_ALIGN_TOP_LEFT, 4, wide ? 64 : 28);

    btn_holo = lv_btn_create(card);
    if (wide) {
        lv_obj_set_size(btn_holo, UI_W - MAP_W - 32 - 28, 64);
        lv_obj_align(btn_holo, LV_ALIGN_BOTTOM_MID, 0, 0);
    } else {
        lv_obj_set_size(btn_holo, 64, 36);
        lv_obj_align(btn_holo, LV_ALIGN_RIGHT_MID, -4, 0);
    }
    ui_style_btn(btn_holo, false);
    lv_obj_add_event_cb(btn_holo, event_holo_btn, LV_EVENT_CLICKED, NULL);
    lv_obj_t* lh = lv_label_create(btn_holo);
    lv_label_set_text(lh, tr(S_MAP_HOLO));
    lv_obj_set_style_text_font(lh, ui_font(wide ? 16 : 12), 0);
    lv_obj_center(lh);

    map_show_sel();
    if (!map_timer) map_timer = lv_timer_create(map_timer_cb, 120, NULL);
    return scr_map;
}
