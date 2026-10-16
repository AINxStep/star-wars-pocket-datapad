#include "../ui_common.h"
#include "rebel_logo.h"

static lv_obj_t* scr_splash;
static lv_obj_t* bar_boot;
static lv_obj_t* lbl_boot;
static lv_timer_t* splash_timer;
static int splash_step = 0;

// =========================================================================
// 0. Splash Screen (Rebel crest + boot sequence)
// =========================================================================
static const StrId boot_msgs[] = { S_BOOT_1, S_BOOT_2, S_BOOT_3, S_BOOT_4 };
#define BOOT_STEPS 4

static void logo_opa_anim_cb(void* obj, int32_t v) {
    lv_obj_set_style_img_opa((lv_obj_t*)obj, (lv_opa_t)v, 0);
}

static void splash_timer_cb(lv_timer_t* t) {
    if (splash_step < BOOT_STEPS) {
        lv_label_set_text(lbl_boot, tr(boot_msgs[splash_step]));
        lv_bar_set_value(bar_boot, (splash_step + 1) * 100 / BOOT_STEPS, LV_ANIM_ON);
        splash_step++;
    } else {
        lv_timer_del(splash_timer);
        splash_timer = NULL;
        ui_goto_home();
    }
}

static void event_splash_skip(lv_event_t* e) {
    if (splash_timer) {
        lv_timer_del(splash_timer);
        splash_timer = NULL;
        ui_goto_home();
    }
}

lv_obj_t* screen_splash_create(void) {
    scr_splash = lv_obj_create(NULL);
    lv_obj_add_style(scr_splash, &style_base, 0);
    lv_obj_clear_flag(scr_splash, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(scr_splash, event_splash_skip, LV_EVENT_CLICKED, NULL);

    lv_obj_t* logo = lv_img_create(scr_splash);
    lv_img_set_src(logo, &rebel_logo);
    lv_obj_set_style_img_recolor(logo, COLOR_ACCENT, 0);
    lv_obj_set_style_img_recolor_opa(logo, LV_OPA_COVER, 0);
    lv_obj_set_style_img_opa(logo, LV_OPA_TRANSP, 0);
    const bool wide = UI_WIDE;
    if (wide) lv_img_set_zoom(logo, 640);          // 80 px bitmap drawn at 2.5x, around its centre
    lv_obj_align(logo, LV_ALIGN_TOP_MID, 0, wide ? 110 : 56);

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, logo);
    lv_anim_set_exec_cb(&a, logo_opa_anim_cb);
    lv_anim_set_values(&a, LV_OPA_TRANSP, LV_OPA_COVER);
    lv_anim_set_time(&a, 900);
    lv_anim_start(&a);

    lv_obj_t* title = lv_label_create(scr_splash);
    lv_label_set_text(title, "REBEL DATAPAD");
    lv_obj_set_style_text_font(title, ui_font(lang_get() == LANG_AUR ? 16 : 20), 0);   // glyphs are wider
    lv_obj_set_style_text_color(title, COLOR_ACCENT, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, wide ? 290 : 150);

    lv_obj_t* sub = lv_label_create(scr_splash);
    lv_label_set_text(sub, tr(S_SPLASH_SUB));
    lv_obj_set_style_text_font(sub, ui_font(12), 0);
    lv_obj_set_style_text_color(sub, COLOR_DATA, 0);
    lv_obj_align(sub, LV_ALIGN_TOP_MID, 0, wide ? 345 : 180);

    bar_boot = lv_bar_create(scr_splash);
    lv_obj_set_size(bar_boot, wide ? 360 : 180, wide ? 12 : 8);
    lv_obj_align(bar_boot, LV_ALIGN_BOTTOM_MID, 0, wide ? -70 : -52);
    lv_bar_set_range(bar_boot, 0, 100);
    lv_bar_set_value(bar_boot, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(bar_boot, COLOR_PANEL, LV_PART_MAIN);
    lv_obj_set_style_bg_color(bar_boot, COLOR_ACCENT, LV_PART_INDICATOR);
    lv_obj_set_style_radius(bar_boot, 2, LV_PART_MAIN);
    lv_obj_set_style_radius(bar_boot, 2, LV_PART_INDICATOR);

    lbl_boot = lv_label_create(scr_splash);
    lv_label_set_text(lbl_boot, "");
    lv_obj_set_style_text_font(lbl_boot, ui_font(12), 0);
    lv_obj_set_style_text_color(lbl_boot, COLOR_TEXT, 0);
    lv_obj_align(lbl_boot, LV_ALIGN_BOTTOM_MID, 0, wide ? -32 : -28);

    splash_timer = lv_timer_create(splash_timer_cb, 550, NULL);
    return scr_splash;
}
