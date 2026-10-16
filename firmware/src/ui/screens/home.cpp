#include "../ui_common.h"

static lv_obj_t* scr_home;

// -------------------------------------------------------------------------
// Ambient comlink chatter: while Home is on screen a timer fires at a random
// interval (1-5 min) and plays one of the /sounds/coms*.wav clips at random.
// The timer is created once (it survives the screen rebuilds of a language
// change) and skips its tick when a UI effect is already playing so it never
// cuts a click or an alarm short.
// -------------------------------------------------------------------------
static lv_timer_t* coms_timer = NULL;

// Shuffle bag: every round plays the three clips in a random order before any
// repeats, and a new round never starts with the clip that ended the last one.
// Independent random picks repeat the same clip back to back one time in three,
// which at one clip every few minutes reads as "always the same sound".
static int next_coms_clip(void) {
    static int bag[3];
    static int pos = 3;                    // 3 = empty, refill on first use
    static int last = -1;
    if (pos >= 3) {
        for (int i = 0; i < 3; i++) bag[i] = i;
        for (int i = 2; i > 0; i--) {      // Fisher-Yates
            int j = random(i + 1);
            int tmp = bag[i]; bag[i] = bag[j]; bag[j] = tmp;
        }
        if (bag[0] == last) { int tmp = bag[0]; bag[0] = bag[1]; bag[1] = tmp; }
        pos = 0;
    }
    last = bag[pos++];
    return last;
}

static void coms_timer_cb(lv_timer_t* t) {
    uint32_t next = 60000 + random(240001);           // next burst in 1-5 min
    lv_timer_set_period(t, next);
    if (lv_scr_act() != scr_home || audio_busy()) {
        Serial.printf("[AMBIENT] skipped (%s), next in %u s\n", lv_scr_act() != scr_home ? "not on Home" : "audio busy", (unsigned)(next / 1000));
        return;
    }
    int pick = next_coms_clip();
    uint32_t ms = sfx_coms(pick);
    Serial.printf("[AMBIENT] coms%d (%u ms), next in %u s\n", pick + 1, (unsigned)ms, (unsigned)(next / 1000));
}

// Each button carries its target in user_data (0..3 grid, 4 settings,
// 5 starchart, 6 beacon)
static void event_btn_home_handler(lv_event_t* e) {
    uint32_t target = (uint32_t)(uintptr_t)lv_event_get_user_data(e);
    sfx_click();

    switch (target) {
        case 0: ui_goto_radar(); break;
        case 1: ui_goto_comlink(); break;
        case 2: ui_goto_aurebesh(); break;
        case 3: ui_goto_holo(); break;
        case 4: ui_goto_settings(); break;
        case 5: ui_goto_map(); break;
        case 6: ui_goto_beacon(); break;
    }
}

// =========================================================================
// 1. Home Screen (Rebel OS Hub)
// =========================================================================
lv_obj_t* screen_home_create(void) {
    scr_home = lv_obj_create(NULL);
    lv_obj_add_style(scr_home, &style_base, 0);
    lv_obj_clear_flag(scr_home, LV_OBJ_FLAG_SCROLLABLE);
    ui_attach_top_bar(scr_home, "REBEL DATAPAD", false);

    const bool wide = UI_WIDE;

    // Sub-header Badge
    lv_obj_t* badge = lv_label_create(scr_home);
    lv_label_set_text(badge, tr(S_HOME_BADGE));
    lv_obj_align(badge, LV_ALIGN_TOP_MID, 0, TOP_BAR_H + (wide ? 18 : 8));
    lv_obj_set_style_text_color(badge, COLOR_DATA, 0);
    lv_obj_set_style_text_font(badge, ui_font(12), 0);

    // Tactical grid: 2x2 on the portrait layout, one row of four when wide
    const char* titles[4] = {tr(S_HOME_RADAR), tr(S_HOME_COMLINK), tr(S_HOME_AUREBESH), tr(S_HOME_HOLO)};
    const int gap = wide ? 20 : 12;
    const int bw = wide ? (UI_W - 2 * 30 - 3 * gap) / 4 : 102;
    const int bh = wide ? 190 : 88;

    for (int i = 0; i < 4; i++) {
        lv_obj_t* btn = lv_btn_create(scr_home);
        lv_obj_set_size(btn, bw, bh);
        if (wide) lv_obj_set_pos(btn, 30 + i * (bw + gap), TOP_BAR_H + 80);
        else      lv_obj_set_pos(btn, 12 + (i % 2) * 114, 76 + (i / 2) * 98);
        ui_style_btn(btn, false);
        lv_obj_add_event_cb(btn, event_btn_home_handler, LV_EVENT_CLICKED, (void*)(uintptr_t)i);

        lv_obj_t* label = lv_label_create(btn);
        lv_label_set_text(label, titles[i]);
        lv_obj_center(label);
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
        if (wide) lv_obj_set_style_text_font(label, ui_font(16), 0);
    }

    // Utility row: settings, starchart and signal beacon. Icon-only buttons
    // are immune to the extra width of Aurebesh glyphs.
    static const char* icons[3] = { LV_SYMBOL_SETTINGS, LV_SYMBOL_GPS, LV_SYMBOL_WIFI };
    const int ub_w = wide ? 110 : 68, ub_h = wide ? 60 : 40, ub_gap = wide ? 16 : 6;
    for (int i = 0; i < 3; i++) {
        lv_obj_t* btn = lv_btn_create(scr_home);
        lv_obj_set_size(btn, ub_w, ub_h);
        lv_obj_align(btn, LV_ALIGN_BOTTOM_LEFT, (wide ? 30 : 12) + i * (ub_w + ub_gap), wide ? -24 : -12);
        ui_style_btn(btn, false);
        lv_obj_add_event_cb(btn, event_btn_home_handler, LV_EVENT_CLICKED, (void*)(uintptr_t)(4 + i));

        lv_obj_t* l = lv_label_create(btn);
        lv_label_set_text(l, icons[i]);
        lv_obj_center(l);
        if (wide) lv_obj_set_style_text_font(l, ui_font_latin(20), 0);
    }

    if (!coms_timer) coms_timer = lv_timer_create(coms_timer_cb, 60000 + random(240001), NULL);
    return scr_home;
}
