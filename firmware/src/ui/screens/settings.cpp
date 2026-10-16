#include "../ui_common.h"
#include "aurebesh_font.h"

static lv_obj_t* scr_settings;
static lv_obj_t* body;

// =========================================================================
// 6. Settings Screen (brightness, volume, language, theme, power, calibration)
// The rows live in a vertically scrollable body under the fixed top bar.
// =========================================================================
// Current column: left edge and width (portrait: one column; wide: two)
static int cx = 16, cw = 208;
static inline bool wide_ui(void) { return UI_WIDE; }

static lv_obj_t* lbl_bright_val;
static lv_obj_t* lbl_volume_val;

// Last slider values confirmed by a clean release, so a press whose drag
// turned into a scroll can be rolled back instead of keeping the jumped value
static int committed_bright;
static int committed_volume;

// True while the finger that pressed a control is scrolling the body instead
static bool press_became_scroll(void) {
    lv_indev_t* indev = lv_indev_get_act();
    return indev && lv_indev_get_scroll_obj(indev) != NULL;
}

static void style_slider(lv_obj_t* sl) {
    lv_obj_set_style_bg_color(sl, COLOR_PANEL, LV_PART_MAIN);
    lv_obj_set_style_bg_color(sl, COLOR_ACCENT, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(sl, COLOR_ACCENT, LV_PART_KNOB);
    lv_obj_set_style_pad_all(sl, wide_ui() ? 8 : 4, LV_PART_KNOB);
}

static void show_brightness(int v) {
    if (v == 0) lv_label_set_text(lbl_bright_val, tr(S_AUTO));
    else        lv_label_set_text_fmt(lbl_bright_val, "%d%%", v);
}

static void event_brightness_handler(lv_event_t* e) {
    lv_obj_t* sl = lv_event_get_target(e);
    int v = lv_slider_get_value(sl);
    if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED) {
        settings().brightness = v;      // 0 = AUTO (LDR), resolved in main.cpp
        if (BOARD_BRIGHTNESS_LIVE) app_apply_brightness(v);   // else applied on release (full repaint)
        show_brightness(v);
    } else if (press_became_scroll()) {
        settings().brightness = committed_bright;   // restore the pre-press value
        app_apply_brightness(committed_bright);
        show_brightness(committed_bright);
        lv_slider_set_value(sl, committed_bright, LV_ANIM_OFF);
    } else {
        committed_bright = v;
        if (!BOARD_BRIGHTNESS_LIVE) app_apply_brightness(v);
        settings_save_soon();   // persist after the slider settles (spares flash writes)
    }
}

static void event_volume_handler(lv_event_t* e) {
    lv_obj_t* sl = lv_event_get_target(e);
    int v = lv_slider_get_value(sl);
    if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED) {
        settings().volume = v;
        audio_set_volume(v);
        lv_label_set_text_fmt(lbl_volume_val, "%d%%", v);
    } else if (press_became_scroll()) {
        settings().volume = committed_volume;
        audio_set_volume(committed_volume);
        lv_label_set_text_fmt(lbl_volume_val, "%d%%", committed_volume);
        lv_slider_set_value(sl, committed_volume, LV_ANIM_OFF);
    } else {
        committed_volume = v;
        settings_save_soon();
        sfx_comlink_chirp();   // audible preview of the new level
    }
}

static void event_calibrate_handler(lv_event_t* e) {
    if (!ui_click_clean()) return;      // ignore drags that were meant to scroll
    sfx_click();
    ui_goto_calibration();
}

static void event_restart_handler(lv_event_t* e) {
    if (!ui_click_clean()) return;      // ignore drags that were meant to scroll
    sfx_click();
    settings_save();                    // flush any pending slider write
    delay(120);                         // let the click be heard
    ESP.restart();
}

// Thin section divider
static void add_divider(int y) {
    lv_obj_t* d = lv_obj_create(body);
    lv_obj_set_size(d, cw, 1);
    lv_obj_set_pos(d, cx, y);
    lv_obj_set_style_bg_color(d, COLOR_ACCENT, 0);
    lv_obj_set_style_bg_opa(d, LV_OPA_20, 0);
    lv_obj_set_style_border_width(d, 0, 0);
    lv_obj_set_style_radius(d, 0, 0);
    lv_obj_set_style_pad_all(d, 0, 0);
    lv_obj_clear_flag(d, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
}

// Toggle switch: checked state is stored as 0/1 in the field
static void event_toggle_handler(lv_event_t* e) {
    lv_obj_t* sw = lv_event_get_target(e);
    uint8_t* field = (uint8_t*)lv_event_get_user_data(e);
    if (!ui_click_clean()) {
        // A drag meant to scroll leaked through as a click: LVGL already
        // flipped the CHECKED state internally, so revert it visually
        if (*field) lv_obj_add_state(sw, LV_STATE_CHECKED);
        else        lv_obj_clear_state(sw, LV_STATE_CHECKED);
        return;
    }
    *field = lv_obj_has_state(sw, LV_STATE_CHECKED) ? 1 : 0;
    settings_save();
    sfx_click();
}

static void add_setting_row(const char* title, int min, int max, int value, lv_obj_t** val_lbl,
                            lv_event_cb_t cb, int y) {
    const bool w = wide_ui();
    lv_obj_t* t = lv_label_create(body);
    lv_label_set_text(t, title);
    lv_obj_set_style_text_font(t, ui_font(12), 0);
    lv_obj_set_style_text_color(t, COLOR_DATA, 0);
    lv_obj_set_pos(t, cx, y);

    *val_lbl = lv_label_create(body);
    if (cb == event_brightness_handler) show_brightness(value);
    else lv_label_set_text_fmt(*val_lbl, "%d%%", value);
    lv_obj_set_style_text_font(*val_lbl, ui_font(12), 0);
    lv_obj_set_style_text_color(*val_lbl, COLOR_ACCENT, 0);
    lv_obj_set_width(*val_lbl, 120);
    lv_obj_set_style_text_align(*val_lbl, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_pos(*val_lbl, cx + cw - 120, y);

    lv_obj_t* sl = lv_slider_create(body);
    lv_obj_set_size(sl, cw, w ? 20 : 12);
    lv_obj_set_pos(sl, cx, y + (w ? 40 : 26));
    lv_slider_set_range(sl, min, max);
    lv_slider_set_value(sl, value, LV_ANIM_OFF);
    style_slider(sl);
    lv_obj_add_event_cb(sl, cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(sl, cb, LV_EVENT_RELEASED, NULL);
}

static void style_switch(lv_obj_t* sw) {
    lv_obj_set_style_bg_color(sw, COLOR_PANEL, LV_PART_MAIN);
    lv_obj_set_style_bg_color(sw, COLOR_ACCENT, LV_PART_INDICATOR);
    lv_obj_set_style_border_color(sw, COLOR_ACCENT, LV_PART_MAIN);
    lv_obj_set_style_border_width(sw, 1, LV_PART_MAIN);
}

// Label on the left, on/off switch on the right
static void add_switch_row(const char* title, uint8_t* field, int y) {
    const bool w = wide_ui();
    const int sw_w = w ? 84 : 52, sw_h = w ? 42 : 26;
    lv_obj_t* t = lv_label_create(body);
    lv_label_set_text(t, title);
    lv_obj_set_style_text_font(t, ui_font(12), 0);
    lv_obj_set_style_text_color(t, COLOR_DATA, 0);
    lv_obj_set_pos(t, cx, y + (w ? 8 : 6));

    lv_obj_t* sw = lv_switch_create(body);
    lv_obj_set_size(sw, sw_w, sw_h);
    lv_obj_set_pos(sw, cx + cw - sw_w, y);
    style_switch(sw);
    if (*field) lv_obj_add_state(sw, LV_STATE_CHECKED);
    ui_click_guard(sw);
    lv_obj_add_event_cb(sw, event_toggle_handler, LV_EVENT_VALUE_CHANGED, field);
}

static void event_lang_handler(lv_event_t* e) {
    if (!ui_click_clean()) return;      // ignore drags that were meant to scroll
    Lang l = (Lang)(uintptr_t)lv_event_get_user_data(e);
    if (l == lang_get()) return;
    sfx_click();
    settings().language = l;
    settings_save();
    ui_apply_language();   // rebuilds every screen with the new language
}

// Selector with one button per language; labels stay Latin so the current
// language can always be changed back, except AUR which shows its own script
static void add_language_selector(int y) {
    const bool w = wide_ui();
    lv_obj_t* title = lv_label_create(body);
    lv_label_set_text(title, "IDIOMA / LANGUAGE");
    lv_obj_set_style_text_font(title, ui_font_latin(12), 0);
    lv_obj_set_style_text_color(title, COLOR_DATA, 0);
    lv_obj_set_pos(title, cx, y);

    static const char* names[LANG_COUNT] = { "ES", "EN", "AUR" };
    const int gap = w ? 14 : 9;
    const int bw = (cw - (LANG_COUNT - 1) * gap) / LANG_COUNT, bh = w ? 64 : 36;
    for (int i = 0; i < LANG_COUNT; i++) {
        lv_obj_t* btn = lv_btn_create(body);
        lv_obj_set_size(btn, bw, bh);
        lv_obj_set_pos(btn, cx + i * (bw + gap), y + (w ? 34 : 20));
        ui_style_btn(btn, false);
        lv_obj_add_style(btn, &style_btn_tactical_pressed, LV_STATE_CHECKED);   // selected = amber
        if (i == (int)lang_get()) lv_obj_add_state(btn, LV_STATE_CHECKED);
        ui_click_guard(btn);
        lv_obj_add_event_cb(btn, event_lang_handler, LV_EVENT_CLICKED, (void*)(uintptr_t)i);

        lv_obj_t* lbl = lv_label_create(btn);
        lv_obj_center(lbl);
#ifdef HAVE_AUREBESH_FONT
        if (i == LANG_AUR) {   // one big glyph reads better than a cramped word
            lv_label_set_text(lbl, "A");
            lv_obj_set_style_text_font(lbl, w ? &font_aurebesh_40 : &font_aurebesh_28, 0);
        } else
#endif
        {
            lv_label_set_text(lbl, names[i]);
            lv_obj_set_style_text_font(lbl, ui_font_latin(14), 0);
        }
    }
}

static void event_theme_handler(lv_event_t* e) {
    if (!ui_click_clean()) return;      // ignore drags that were meant to scroll
    int t = (int)(uintptr_t)lv_event_get_user_data(e);
    if (t == settings().theme) return;
    sfx_click();
    settings().theme = t;
    settings_save();
    ui_apply_language();   // rebuilds styles and every screen with the new colors
}

// Two buttons, one per theme; the selected one is highlighted
static void add_theme_selector(int y) {
    const bool w = wide_ui();
    lv_obj_t* title = lv_label_create(body);
    lv_label_set_text(title, "TEMA / THEME");
    lv_obj_set_style_text_font(title, ui_font_latin(12), 0);
    lv_obj_set_style_text_color(title, COLOR_DATA, 0);
    lv_obj_set_pos(title, cx, y);

    static const char* names[THEME_COUNT] = { "TATOOINE", "IMPERIAL" };
    const int gap = w ? 14 : 12;
    const int bw = (cw - (THEME_COUNT - 1) * gap) / THEME_COUNT, bh = w ? 64 : 36;
    for (int i = 0; i < THEME_COUNT; i++) {
        lv_obj_t* btn = lv_btn_create(body);
        lv_obj_set_size(btn, bw, bh);
        lv_obj_set_pos(btn, cx + i * (bw + gap), y + (w ? 34 : 20));
        ui_style_btn(btn, false);
        lv_obj_add_style(btn, &style_btn_tactical_pressed, LV_STATE_CHECKED);   // selected = accent
        if (i == settings().theme) lv_obj_add_state(btn, LV_STATE_CHECKED);
        ui_click_guard(btn);
        lv_obj_add_event_cb(btn, event_theme_handler, LV_EVENT_CLICKED, (void*)(uintptr_t)i);

        lv_obj_t* lbl = lv_label_create(btn);
        lv_label_set_text(lbl, names[i]);
        lv_obj_set_style_text_font(lbl, ui_font_latin(14), 0);   // always Latin
        lv_obj_center(lbl);
    }
}

static void add_calibrate_button(int y) {
    lv_obj_t* btn = lv_btn_create(body);
    lv_obj_set_size(btn, cw, wide_ui() ? 64 : 40);
    lv_obj_set_pos(btn, cx, y);
    ui_style_btn(btn, false);
    ui_click_guard(btn);
    lv_obj_add_event_cb(btn, event_calibrate_handler, LV_EVENT_CLICKED, NULL);
    lv_obj_t* lbl = lv_label_create(btn);
    lv_label_set_text(lbl, tr(S_SET_CALIBRATE));
    lv_obj_center(lbl);
}

static void add_restart_button(int y) {
    lv_obj_t* btn = lv_btn_create(body);
    lv_obj_set_size(btn, cw, wide_ui() ? 64 : 40);
    lv_obj_set_pos(btn, cx, y);
    ui_style_btn(btn, false);
    ui_click_guard(btn);
    lv_obj_add_event_cb(btn, event_restart_handler, LV_EVENT_CLICKED, NULL);
    lv_obj_t* lbl = lv_label_create(btn);
    lv_label_set_text(lbl, tr(S_SET_RESTART));
    lv_obj_center(lbl);
}

static void settings_shadow_cb(lv_event_t* e) {
    board_dim_shadow(lv_event_get_code(e) == LV_EVENT_SCREEN_LOAD_START);
}

lv_obj_t* screen_settings_create(void) {
    const bool wide = wide_ui();
    scr_settings = lv_obj_create(NULL);
    lv_obj_add_event_cb(scr_settings, settings_shadow_cb, LV_EVENT_SCREEN_LOAD_START, NULL);
    lv_obj_add_event_cb(scr_settings, settings_shadow_cb, LV_EVENT_SCREEN_UNLOAD_START, NULL);
    lv_obj_add_style(scr_settings, &style_base, 0);
    lv_obj_clear_flag(scr_settings, LV_OBJ_FLAG_SCROLLABLE);
    ui_attach_top_bar(scr_settings, tr(S_SETTINGS), true);

    // Body under the fixed top bar: scrollable column on the portrait layout,
    // two static columns when wide
    body = lv_obj_create(scr_settings);
    lv_obj_set_size(body, UI_W, UI_H - TOP_BAR_H);
    lv_obj_align(body, LV_ALIGN_TOP_MID, 0, TOP_BAR_H);
    lv_obj_set_style_bg_opa(body, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(body, 0, 0);
    lv_obj_set_style_pad_all(body, 0, 0);
    if (wide) {
        lv_obj_clear_flag(body, LV_OBJ_FLAG_SCROLLABLE);
    } else {
        lv_obj_set_scroll_dir(body, LV_DIR_VER);
        lv_obj_set_scrollbar_mode(body, LV_SCROLLBAR_MODE_AUTO);
        // No fling: noisy resistive vectors make the momentum jump around;
        // the content follows the finger exactly and stops on release
        lv_obj_clear_flag(body, LV_OBJ_FLAG_SCROLL_MOMENTUM);
    }

    committed_bright = settings().brightness;
    committed_volume = settings().volume;

    if (wide) {
        // Left column: levels and power
        cx = 40; cw = 340;
        add_setting_row(tr(S_SET_BRIGHTNESS), 0, 100, settings().brightness, &lbl_bright_val,
                        event_brightness_handler, 18);
        add_setting_row(tr(S_SET_VOLUME), 0, 100, settings().volume, &lbl_volume_val,
                        event_volume_handler, 108);
        add_divider(200);
        add_switch_row(tr(S_SET_SLEEP), &settings().sleep_en, 222);
        add_switch_row(tr(S_SET_BATT), &settings().batt_en, 286);

        // Right column: language, theme, calibration
        cx = 440; cw = 320;
        add_language_selector(18);
        add_divider(128);
        add_theme_selector(146);
        add_divider(256);
        if (BOARD_HAS_RESISTIVE) add_calibrate_button(274);
        add_restart_button(BOARD_HAS_RESISTIVE ? 346 : 274);
    } else {
        cx = 16; cw = 208;
        add_setting_row(tr(S_SET_BRIGHTNESS), 0, 100, settings().brightness, &lbl_bright_val,
                        event_brightness_handler, 10);
        add_setting_row(tr(S_SET_VOLUME), 0, 100, settings().volume, &lbl_volume_val,
                        event_volume_handler, 70);

        add_divider(118);
        add_language_selector(128);

        add_divider(192);
        add_theme_selector(200);

        add_divider(264);
        add_switch_row(tr(S_SET_SLEEP), &settings().sleep_en, 272);
        add_switch_row(tr(S_SET_BATT), &settings().batt_en, 310);

        add_divider(344);
        cx = 12; cw = 216;
        if (BOARD_HAS_RESISTIVE) add_calibrate_button(352);
        add_restart_button(BOARD_HAS_RESISTIVE ? 400 : 352);
    }

    if (BOARD_HAS_RESISTIVE) {   // the hint is about the BOOT-hold calibration shortcut
        lv_obj_t* hint = lv_label_create(body);
        lv_label_set_text(hint, tr(S_SET_HINT));
        lv_obj_set_style_text_font(hint, ui_font(12), 0);
        lv_obj_set_style_text_color(hint, COLOR_TEXT, 0);
        lv_obj_set_style_text_align(hint, LV_TEXT_ALIGN_CENTER, 0);
        if (wide) lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -16);
        else      lv_obj_align(hint, LV_ALIGN_TOP_MID, 0, 452);
    }
    return scr_settings;
}
