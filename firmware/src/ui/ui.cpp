#include "ui_common.h"

// =========================================================================
// UI lifecycle and screen navigation
// =========================================================================

static lv_obj_t* screens[SCR_COUNT];

lv_obj_t* ui_screen(UiScreen id) { return screens[id]; }

// Creates every screen except the splash, which only exists at boot
static void create_screens(void) {
    screens[SCR_HOME]     = screen_home_create();
    screens[SCR_RADAR]    = screen_radar_create();
    screens[SCR_COMLINK]  = screen_comlink_create();
    screens[SCR_HOLO]     = screen_holo_create();
    screens[SCR_AUREBESH] = screen_aurebesh_create();
    screens[SCR_SETTINGS] = screen_settings_create();
    screens[SCR_CALIB]    = screen_calibration_create();
    screens[SCR_MAP]      = screen_starchart_create();
    screens[SCR_BEACON]   = screen_beacon_create();
}

void ui_init(void) {
    ui_styles_init();
    create_screens();
    screens[SCR_SPLASH] = screen_splash_create();
    ui_msg_alert_init();

    // Boot on the splash screen; it hands over to Home when finished
    lv_scr_load(screens[SCR_SPLASH]);
}

// Language change: all text and fonts are fixed when a screen is built, so the
// screens are recreated. The old ones are deleted first because the LVGL
// memory pool cannot hold two full sets, so a blank screen is shown meanwhile.
static void rebuild_cb(void*) {
    lv_obj_t* blank = lv_obj_create(NULL);
    lv_scr_load(blank);

    ui_bat_labels_reset();              // labels point into the deleted screens
    for (int i = 0; i < SCR_COUNT; i++) {
        if (i == SCR_SPLASH || !screens[i]) continue;
        lv_obj_del(screens[i]);
        screens[i] = NULL;
    }

    ui_styles_init();       // refreshes the default font for the new language
    create_screens();

    lv_scr_load(screens[SCR_SETTINGS]);
    lv_obj_del(blank);
}

void ui_apply_language(void) { lv_async_call(rebuild_cb, NULL); }

void ui_goto(UiScreen id) {
    lv_obj_t* target = screens[id];
    lv_obj_t* current = lv_scr_act();
    if (current == target) return;

    // Home slides back (or fades in from the splash); everything else slides forward
    lv_scr_load_anim_t anim = LV_SCR_LOAD_ANIM_MOVE_LEFT;
    if (id == SCR_HOME) {
        anim = current == screens[SCR_SPLASH] ? LV_SCR_LOAD_ANIM_FADE_ON : LV_SCR_LOAD_ANIM_MOVE_RIGHT;
    }
    // Full-screen slides tear on the wide RGB panel (each frame is a full
    // 800x480 redraw, slower than the panel's refresh): cut straight over there
    if (UI_WIDE) anim = LV_SCR_LOAD_ANIM_NONE;
    lv_scr_load_anim(target, anim, UI_WIDE ? 0 : 220, 0, false);
}

void ui_goto_home(void)      { ui_goto(SCR_HOME); }
void ui_goto_radar(void)     { ui_goto(SCR_RADAR); }
void ui_goto_comlink(void)   { ui_goto(SCR_COMLINK); }
void ui_goto_holo(void)      { ui_goto(SCR_HOLO); }
void ui_goto_aurebesh(void)  { ui_goto(SCR_AUREBESH); }
void ui_goto_settings(void)  { ui_goto(SCR_SETTINGS); }
void ui_goto_map(void)       { ui_goto(SCR_MAP); }
void ui_goto_beacon(void)    { ui_goto(SCR_BEACON); }

void ui_goto_calibration(void) {
    if (lv_scr_act() == screens[SCR_CALIB]) return;
    ui_calibration_begin();
    lv_scr_load_anim(screens[SCR_CALIB], UI_WIDE ? LV_SCR_LOAD_ANIM_NONE : LV_SCR_LOAD_ANIM_FADE_ON,
                     UI_WIDE ? 0 : 150, 0, false);
}
