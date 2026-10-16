#pragma once
#include <lvgl.h>
#include "ui.h"
#include "starwars_audio.h"
#include "hardware.h"
#include "settings.h"
#include "i18n.h"

// =========================================================================
// Shared UI toolkit: theme, styles, top bar and the screen registry.
// To add a screen: create src/ui/screens/<name>.cpp exposing
// `lv_obj_t* screen_<name>_create(void)`, add an entry to UiScreen below and
// to the table in ui.cpp, then navigate with ui_goto(SCR_<NAME>).
// =========================================================================

// Color themes (selected in Settings, stored in NVS). The COLOR_* roles are
// semantic, not literal: ACCENT is amber on the Tatooine theme and red on the
// Imperial one, ALERT is the warning color (hostile contacts, red alert).
struct UiTheme {
    lv_color_t bg, panel;
    lv_color_t accent;      // borders, titles, selected controls
    lv_color_t data;        // secondary labels and values
    lv_color_t alert;       // hostile contacts, red alert
    lv_color_t friendly;    // allied contacts
    lv_color_t text;
    lv_color_t holo;        // hologram and radar projection lines
    lv_color_t chamber;     // projection chamber background
    lv_color_t hilite;      // bright projection highlights
};

enum UiThemeId { THEME_TATOOINE, THEME_IMPERIAL, THEME_COUNT };

static inline const UiTheme& ui_theme(void) {
    static const UiTheme themes[THEME_COUNT] = {
        { // Tatooine: dark desert brown, twin-sun amber, turquoise oasis; classic blue holograms
            lv_color_make(22, 15, 9), lv_color_make(48, 34, 22),
            lv_color_make(255, 179, 71), lv_color_make(63, 201, 176),
            lv_color_make(255, 64, 48), lv_color_make(110, 220, 110),
            lv_color_make(250, 238, 215),
            lv_color_make(70, 205, 255), lv_color_make(2, 9, 15), lv_color_make(190, 245, 255) },
        { // Imperial: near-black steel, imperial red, ice grey; amber warnings
            lv_color_make(8, 9, 11), lv_color_make(26, 29, 34),
            lv_color_make(224, 32, 32), lv_color_make(184, 196, 204),
            lv_color_make(255, 176, 0), lv_color_make(80, 205, 130),
            lv_color_make(228, 233, 238),
            lv_color_make(165, 210, 235), lv_color_make(5, 8, 11), lv_color_make(225, 240, 248) },
    };
    return themes[settings().theme < THEME_COUNT ? settings().theme : 0];
}

#define COLOR_BG        (ui_theme().bg)
#define COLOR_PANEL     (ui_theme().panel)
#define COLOR_ACCENT    (ui_theme().accent)
#define COLOR_DATA      (ui_theme().data)
#define COLOR_ALERT     (ui_theme().alert)
#define COLOR_FRIEND    (ui_theme().friendly)
#define COLOR_TEXT      (ui_theme().text)
#define COLOR_HOLO      (ui_theme().holo)
#define COLOR_CHAMBER   (ui_theme().chamber)
#define COLOR_HILITE    (ui_theme().hilite)

// Screen geometry. The UI supports portrait (ES3C28P, 240x320) and wide
// landscape layouts. Screens query the live display size.
#define UI_W     ((int)lv_disp_get_hor_res(NULL))
#define UI_H     ((int)lv_disp_get_ver_res(NULL))
#define UI_WIDE  (UI_W > UI_H)
#define TOP_BAR_H (UI_WIDE ? 60 : 44)
// Margin and gap used by the wide layouts
#define UI_PAD   (UI_WIDE ? 20 : 12)

// Shared styles (initialized by ui_styles_init)
extern lv_style_t style_base;
extern lv_style_t style_btn_tactical;
extern lv_style_t style_btn_red;
extern lv_style_t style_card;
extern lv_style_t style_btn_tactical_pressed;
extern lv_style_t style_btn_red_pressed;

void ui_styles_init(void);

// Font for a given Montserrat size (12/14/16/20/28): the Aurebesh equivalent
// when the interface language is Aurebesh. On the wide layout every size is
// stepped up (12->18, 14->20, 16->24, 20->28, 28->40) for the bigger panel.
const lv_font_t* ui_font(int montserrat_px);
const lv_font_t* ui_font_latin(int montserrat_px);   // never Aurebesh (symbols, morse, ES/EN labels)

// Rebuilds every screen with the language and theme stored in settings (deferred: safe
// to call from an event handler of a widget that will be deleted)
void ui_apply_language(void);

// Applies the tactical (or red) button style plus its pressed-state feedback
void ui_style_btn(lv_obj_t* btn, bool red);

// Click guard for controls inside scrollable containers: LVGL still delivers
// LV_EVENT_CLICKED to the pressed widget after drags that never started a
// scroll (e.g. diagonal drags on a vertical list). Call ui_click_guard() at
// creation and gate the click handler with ui_click_clean().
void ui_click_guard(lv_obj_t* obj);
bool ui_click_clean(void);

// Top bar with title, optional back button and battery indicator
void ui_attach_top_bar(lv_obj_t* parent, const char* title, bool show_back);

// Drops the battery-label registry (call before deleting/rebuilding screens)
void ui_bat_labels_reset(void);

// Screen registry
enum UiScreen {
    SCR_SPLASH,
    SCR_HOME,
    SCR_RADAR,
    SCR_COMLINK,
    SCR_HOLO,
    SCR_AUREBESH,
    SCR_SETTINGS,
    SCR_CALIB,
    SCR_MAP,
    SCR_BEACON,
    SCR_COUNT
};

lv_obj_t* ui_screen(UiScreen id);
void ui_goto(UiScreen id);   // slides forward; fades from the splash; slides back to Home

// Screen factories (src/ui/screens/*.cpp)
lv_obj_t* screen_splash_create(void);
lv_obj_t* screen_home_create(void);
lv_obj_t* screen_radar_create(void);
lv_obj_t* screen_comlink_create(void);
lv_obj_t* screen_holo_create(void);
lv_obj_t* screen_aurebesh_create(void);
lv_obj_t* screen_settings_create(void);
lv_obj_t* screen_calibration_create(void);
lv_obj_t* screen_starchart_create(void);
lv_obj_t* screen_beacon_create(void);

// Plays a sound-board effect (1 R2-D2, 2 blaster, 3 channel, 4 alarm) with its LED color
void ui_sfx_play(uint8_t id);

// Registers the handler that shows incoming link messages (call once at boot)
void ui_msg_alert_init(void);

// Called by ui_goto_calibration() before the calibration screen is shown
void ui_calibration_begin(void);

// Custom draw callbacks run once per render chunk (LVGL draws in horizontal
// bands): primitives entirely outside the band's clip area are rejected here,
// before paying the cost of a lv_draw_* call.
static inline bool ui_line_clipped(const lv_draw_ctx_t* ctx, int x1, int y1, int x2, int y2, int pad) {
    const lv_area_t* c = ctx->clip_area;
    int lo_y = y1 < y2 ? y1 : y2, hi_y = y1 < y2 ? y2 : y1;
    int lo_x = x1 < x2 ? x1 : x2, hi_x = x1 < x2 ? x2 : x1;
    return hi_y + pad < c->y1 || lo_y - pad > c->y2 || hi_x + pad < c->x1 || lo_x - pad > c->x2;
}
