#include "ui_common.h"
#include "aurebesh_font.h"
#include <stdio.h>
#include <string.h>

lv_style_t style_base;
lv_style_t style_btn_tactical;
lv_style_t style_btn_red;
lv_style_t style_card;
lv_style_t style_btn_tactical_pressed;
lv_style_t style_btn_red_pressed;

// Wide layout: step a requested size up to the large-screen ladder
static int step_px(int px) {
    if (!UI_WIDE) return px;
    switch (px) {
        case 12: return 18;
        case 16: return 24;
        case 20: return 28;
        case 28: return 40;
        default: return 20;
    }
}

static const lv_font_t* latin_font(int px) {
    switch (px) {
        case 12: return &lv_font_montserrat_12;
        case 16: return &lv_font_montserrat_16;
        case 18: return &lv_font_montserrat_18;
        case 20: return &lv_font_montserrat_20;
        case 24: return &lv_font_montserrat_24;
        case 28: return &lv_font_montserrat_28;
        case 40: return &lv_font_montserrat_40;
        default: return &lv_font_montserrat_14;
    }
}

// Latin-only font (morse, symbols, language names...) on the same ladder
const lv_font_t* ui_font_latin(int px) { return latin_font(step_px(px)); }

const lv_font_t* ui_font(int px) {
    px = step_px(px);
#ifdef HAVE_AUREBESH_FONT
    if (lang_get() == LANG_AUR) {
        switch (px) {
            case 12: return &font_aurebesh_12;
            case 16: return &font_aurebesh_16;
            case 18: return &font_aurebesh_18;
            case 20: return &font_aurebesh_20;
            case 24: return &font_aurebesh_24;
            case 28: return &font_aurebesh_28;
            case 40: return &font_aurebesh_40;
            default: return &font_aurebesh_14;
        }
    }
#endif
    return latin_font(px);
}

void ui_styles_init(void) {
    // Called again when the language or theme changes: release the previous properties first
    static bool ready = false;
    if (ready) {
        lv_style_reset(&style_base);
        lv_style_reset(&style_btn_tactical);
        lv_style_reset(&style_btn_red);
        lv_style_reset(&style_card);
        lv_style_reset(&style_btn_tactical_pressed);
        lv_style_reset(&style_btn_red_pressed);
    }
    ready = true;

    lv_style_init(&style_base);
    lv_style_set_bg_color(&style_base, COLOR_BG);
    lv_style_set_text_color(&style_base, COLOR_TEXT);
    lv_style_set_text_font(&style_base, ui_font(14));   // default font of every screen

    lv_style_init(&style_btn_tactical);
    lv_style_set_bg_color(&style_btn_tactical, COLOR_PANEL);
    lv_style_set_border_color(&style_btn_tactical, COLOR_ACCENT);
    lv_style_set_border_width(&style_btn_tactical, 2);
    lv_style_set_radius(&style_btn_tactical, 6);
    lv_style_set_text_color(&style_btn_tactical, COLOR_TEXT);
    lv_style_set_shadow_width(&style_btn_tactical, 0);

    lv_style_init(&style_btn_red);
    lv_style_set_bg_color(&style_btn_red, COLOR_PANEL);
    lv_style_set_border_color(&style_btn_red, COLOR_ALERT);
    lv_style_set_border_width(&style_btn_red, 2);
    lv_style_set_radius(&style_btn_red, 6);
    lv_style_set_text_color(&style_btn_red, COLOR_ALERT);

    lv_style_init(&style_btn_tactical_pressed);
    lv_style_set_bg_color(&style_btn_tactical_pressed, COLOR_ACCENT);
    lv_style_set_bg_opa(&style_btn_tactical_pressed, LV_OPA_COVER);
    lv_style_set_text_color(&style_btn_tactical_pressed, COLOR_BG);

    lv_style_init(&style_btn_red_pressed);
    lv_style_set_bg_color(&style_btn_red_pressed, COLOR_ALERT);
    lv_style_set_bg_opa(&style_btn_red_pressed, LV_OPA_COVER);
    lv_style_set_text_color(&style_btn_red_pressed, COLOR_BG);

    lv_style_init(&style_card);
    lv_style_set_bg_color(&style_card, COLOR_PANEL);
    lv_style_set_border_color(&style_card, COLOR_DATA);
    lv_style_set_border_width(&style_card, 1);
    lv_style_set_radius(&style_card, 4);
}

// Applies the tactical (or red) style plus its pressed-state feedback
void ui_style_btn(lv_obj_t* btn, bool red) {
    lv_obj_add_style(btn, red ? &style_btn_red : &style_btn_tactical, 0);
    lv_obj_add_style(btn, red ? &style_btn_red_pressed : &style_btn_tactical_pressed, LV_STATE_PRESSED);
}

static void event_back_handler(lv_event_t* e) {
    sfx_click();
    ui_goto_home();
}

// Quick rightward swipe on the top bar = back to Home. It is attached only to
// the bar: on the screen background it would fight with interactive controls
// (sliders, scroll containers) that let gestures bubble up to the screen.
static void event_swipe_back(lv_event_t* e) {
    if (lv_indev_get_gesture_dir(lv_indev_get_act()) == LV_DIR_RIGHT) {
        sfx_click();
        ui_goto_home();
    }
}

// -------------------------------------------------------------------------
// Battery indicator: every top bar registers its label so a single timer can
// refresh them all. The registry is rebuilt with the screens (language change
// deletes the old objects, so the pointers must be dropped first).
// -------------------------------------------------------------------------
static lv_obj_t* bat_labels[SCR_COUNT];
static int bat_label_n = 0;
static lv_timer_t* bat_timer = NULL;

void ui_bat_labels_reset(void) { bat_label_n = 0; }

static void bat_timer_cb(lv_timer_t* t) {
    char buf[12];
    if (settings().batt_en) {
        snprintf(buf, sizeof(buf), "%d%%", batt_read_pct());
    } else {
        strcpy(buf, "[||||]");           // decorative until the mod is wired
    }
    for (int i = 0; i < bat_label_n; i++) lv_label_set_text(bat_labels[i], buf);
}

void ui_attach_top_bar(lv_obj_t* parent, const char* title, bool show_back) {
    const bool wide = UI_WIDE;
    const int btn_w = wide ? 120 : 72, btn_h = wide ? 46 : 36;

    lv_obj_t* bar = lv_obj_create(parent);
    lv_obj_set_size(bar, UI_W, TOP_BAR_H);
    lv_obj_align(bar, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(bar, COLOR_PANEL, 0);
    lv_obj_set_style_border_color(bar, COLOR_ACCENT, 0);
    lv_obj_set_style_border_width(bar, 2, 0);
    lv_obj_set_style_border_side(bar, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_pad_all(bar, wide ? 6 : 4, 0);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

    if (show_back) {
        lv_obj_add_event_cb(bar, event_swipe_back, LV_EVENT_GESTURE, NULL);

        lv_obj_t* btn_b = lv_btn_create(bar);
        lv_obj_set_size(btn_b, btn_w, btn_h);
        lv_obj_align(btn_b, LV_ALIGN_LEFT_MID, 0, 0);
        ui_style_btn(btn_b, false);
        lv_obj_add_event_cb(btn_b, event_back_handler, LV_EVENT_CLICKED, NULL);

        lv_obj_t* lbl_b = lv_label_create(btn_b);
        if (lang_get() == LANG_AUR) {
            // Aurebesh glyphs are ~1.7x wider and the font has no arrow glyph:
            // an icon-only button reads fine and never overflows
            lv_label_set_text(lbl_b, LV_SYMBOL_LEFT);
            lv_obj_set_style_text_font(lbl_b, ui_font_latin(14), 0);
        } else {
            lv_label_set_text_fmt(lbl_b, LV_SYMBOL_LEFT " %s", tr(S_BACK));
            lv_obj_set_style_text_font(lbl_b, ui_font(12), 0);
        }
        lv_obj_center(lbl_b);
    }

    lv_obj_t* lbl = lv_label_create(bar);
    lv_label_set_text(lbl, title);
    lv_obj_align(lbl, LV_ALIGN_LEFT_MID, show_back ? btn_w + (wide ? 16 : 8) : (wide ? 12 : 6), 0);
    lv_obj_set_style_text_color(lbl, COLOR_ACCENT, 0);
    lv_obj_set_style_text_font(lbl, ui_font(12), 0);

    lv_obj_t* bat = lv_label_create(bar);
    lv_label_set_text(bat, "[||||]");
    lv_obj_align(bat, LV_ALIGN_RIGHT_MID, wide ? -8 : -4, 0);
    lv_obj_set_style_text_color(bat, COLOR_DATA, 0);
    lv_obj_set_style_text_font(bat, ui_font_latin(14), 0);   // always Latin

    if (bat_label_n < SCR_COUNT) bat_labels[bat_label_n++] = bat;
    if (!bat_timer) bat_timer = lv_timer_create(bat_timer_cb, 5000, NULL);
    bat_timer_cb(NULL);                     // fill the label right away
}

// -------------------------------------------------------------------------
// Click guard (see ui_common.h). The resistive panel produces drags that
// never reach LVGL's scroll_limit; on release LVGL would still deliver
// LV_EVENT_CLICKED to the pressed widget. We record the press point and the
// click is only honored if the finger stayed within a tap's radius.
// -------------------------------------------------------------------------
#define CLICK_DRAG_EPS 14

static lv_point_t press_pt;
static bool press_armed = false;

static void track_press(lv_event_t* e) {
    lv_indev_t* indev = lv_indev_get_act();
    if (!indev) { press_armed = false; return; }
    lv_indev_get_point(indev, &press_pt);
    press_armed = true;
}

void ui_click_guard(lv_obj_t* obj) {
    lv_obj_add_event_cb(obj, track_press, LV_EVENT_PRESSED, NULL);
}

bool ui_click_clean(void) {
    lv_indev_t* indev = lv_indev_get_act();
    if (!indev) { press_armed = false; return true; }
    if (lv_indev_get_scroll_obj(indev)) { press_armed = false; return false; }
    if (!press_armed) return true;
    press_armed = false;
    lv_point_t p;
    lv_indev_get_point(indev, &p);
    int dx = p.x - press_pt.x, dy = p.y - press_pt.y;
    return dx * dx + dy * dy <= CLICK_DRAG_EPS * CLICK_DRAG_EPS;
}
