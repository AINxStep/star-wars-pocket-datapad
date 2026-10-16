#include "ui_common.h"
#include "link.h"

// =========================================================================
// Incoming link message: plays the effect and shows a banner on the top
// layer (above every screen) that closes after a few seconds or on tap.
// =========================================================================
static lv_obj_t* banner = NULL;
static lv_timer_t* hide_timer = NULL;

static void banner_close(void) {
    if (hide_timer) { lv_timer_del(hide_timer); hide_timer = NULL; }
    if (banner) { lv_obj_del(banner); banner = NULL; }
}

static void banner_click(lv_event_t* e) { banner_close(); }
// One-shot timer: LVGL deletes it after this callback, so just forget the handle
static void hide_cb(lv_timer_t* t) {
    hide_timer = NULL;
    banner_close();
}

static void on_link_msg(uint8_t id, const char* mac) {
    static const StrId names[4] = { S_COMLINK_R2D2, S_COMLINK_BLASTER, S_COMLINK_CHANNEL, S_COMLINK_ALARM };
    if (id < 1 || id > 4) return;
    ui_sfx_play(id);

    banner_close();
    const bool wide = UI_WIDE, alert = id == 4;
    const lv_color_t edge = alert ? COLOR_ALERT : COLOR_ACCENT;

    banner = lv_obj_create(lv_layer_top());
    lv_obj_set_size(banner, wide ? 640 : 224, wide ? 96 : 58);
    lv_obj_align(banner, LV_ALIGN_TOP_MID, 0, TOP_BAR_H + (wide ? 16 : 6));
    lv_obj_set_style_bg_color(banner, COLOR_PANEL, 0);
    lv_obj_set_style_bg_opa(banner, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(banner, edge, 0);
    lv_obj_set_style_border_width(banner, 2, 0);
    lv_obj_set_style_radius(banner, 6, 0);
    lv_obj_set_style_pad_all(banner, wide ? 12 : 6, 0);
    lv_obj_clear_flag(banner, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(banner, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(banner, banner_click, LV_EVENT_CLICKED, NULL);

    lv_obj_t* head = lv_label_create(banner);
    lv_label_set_text(head, tr(S_MSG_RX));
    lv_obj_set_style_text_font(head, ui_font(12), 0);
    lv_obj_set_style_text_color(head, COLOR_DATA, 0);
    lv_obj_align(head, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t* from = lv_label_create(banner);     // last three octets of the sender
    lv_label_set_text(from, mac + 9);
    lv_obj_set_style_text_font(from, ui_font_latin(12), 0);
    lv_obj_set_style_text_color(from, COLOR_DATA, 0);
    lv_obj_align(from, LV_ALIGN_TOP_RIGHT, 0, 0);

    lv_obj_t* body = lv_label_create(banner);
    lv_label_set_text(body, tr(names[id - 1]));
    lv_obj_set_style_text_font(body, ui_font(lang_get() == LANG_AUR ? 12 : 16), 0);   // glyphs are wider
    lv_obj_set_style_text_color(body, edge, 0);
    lv_obj_align(body, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    hide_timer = lv_timer_create(hide_cb, 3500, NULL);
    lv_timer_set_repeat_count(hide_timer, 1);
}

void ui_msg_alert_init(void) {
    link_set_msg_handler(on_link_msg);
}
