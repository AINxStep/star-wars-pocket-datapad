#include "../ui_common.h"
#include "link.h"
#include <stdio.h>

static lv_obj_t* scr_comlink;
static lv_obj_t* lbl_link_status;
static lv_timer_t* link_status_timer;

// TX toggle: while on (and the link is active) every effect is also sent to
// the peer. Kept across screen rebuilds.
static bool tx_enabled = true;

// Plays a sound-board effect with its LED color (also used for received messages)
void ui_sfx_play(uint8_t id) {
    switch (id) {
        case 1: hw_led_pulse(false, false, true, sfx_r2d2()); break;
        case 2: hw_led_pulse(true, true, false, sfx_blaster()); break;
        case 3: hw_led_pulse(false, true, true, sfx_comlink_chirp()); break;
        case 4: hw_led_pulse(true, false, false, sfx_red_alert(), 100); break;
    }
}

static void event_sfx_handler(lv_event_t* e) {
    uint8_t sfx_id = (uint8_t)(uintptr_t)lv_event_get_user_data(e);
    ui_sfx_play(sfx_id);
    if (tx_enabled && link_active()) link_send_msg(sfx_id);
}

static void event_tx_toggle(lv_event_t* e) {
    tx_enabled = lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);
    sfx_click();
}

// -------------------------------------------------------------------------
// P2P link row: toggle plus a live status box refreshed once per second
// -------------------------------------------------------------------------
static void link_status_refresh() {
    if (!link_active()) {
        lv_label_set_text(lbl_link_status, tr(S_LINK_OFF));
    } else if (link_peer_seen()) {
        lv_label_set_text(lbl_link_status, tr(S_LINK_FOUND));
    } else {
        lv_label_set_text(lbl_link_status, tr(S_LINK_SEARCH));
    }
}

static void link_status_timer_cb(lv_timer_t* t) {
    if (lv_scr_act() != scr_comlink) return;
    link_status_refresh();
}

static void event_link_toggle(lv_event_t* e) {
    lv_obj_t* btn = lv_event_get_target(e);
    if (lv_obj_has_state(btn, LV_STATE_CHECKED)) link_start();
    else link_stop();
    link_status_refresh();
    sfx_click();
}

// =========================================================================
// 4. Comlink Soundboard Screen + ESP-NOW peer link
// =========================================================================
lv_obj_t* screen_comlink_create(void) {
    scr_comlink = lv_obj_create(NULL);
    lv_obj_add_style(scr_comlink, &style_base, 0);
    lv_obj_clear_flag(scr_comlink, LV_OBJ_FLAG_SCROLLABLE);
    ui_attach_top_bar(scr_comlink, tr(S_COMLINK_TITLE), true);

    const bool wide = UI_WIDE;
    const char* sfx_names[4] = {tr(S_COMLINK_R2D2), tr(S_COMLINK_BLASTER), tr(S_COMLINK_CHANNEL), tr(S_COMLINK_ALARM)};

    // Soundboard: a column of four on the portrait layout, a 2x2 grid on the
    // left of the wide one (the link controls take the right-hand column)
    const int gw = wide ? 260 : 216, gh = wide ? 150 : 50;
    for (int i = 0; i < 4; i++) {
        lv_obj_t* btn = lv_btn_create(scr_comlink);
        lv_obj_set_size(btn, gw, gh);
        if (wide) lv_obj_set_pos(btn, 30 + (i % 2) * (gw + 20), TOP_BAR_H + 30 + (i / 2) * (gh + 20));
        else      lv_obj_set_pos(btn, 12, 50 + i * 56);
        ui_style_btn(btn, i == 3);
        lv_obj_add_event_cb(btn, event_sfx_handler, LV_EVENT_CLICKED, (void*)(uintptr_t)(i + 1));

        lv_obj_t* lbl = lv_label_create(btn);
        lv_label_set_text(lbl, sfx_names[i]);
        lv_obj_center(lbl);
        if (wide) lv_obj_set_style_text_font(lbl, ui_font(16), 0);
    }

    // Link row: P2P toggle (checked while the radio is on; survives screen
    // rebuilds), TX toggle (send effects to the peer) and a live status box.
    // Portrait: one row at the bottom. Wide: stacked in the right column.
    const int col_x = 30 + 2 * (gw + 20) + 20, col_w = UI_W - col_x - 30;
    const int ty = wide ? TOP_BAR_H + 30 : 278;

    lv_obj_t* btn_link = lv_btn_create(scr_comlink);
    lv_obj_set_size(btn_link, wide ? col_w : 68, wide ? 80 : 36);
    lv_obj_set_pos(btn_link, wide ? col_x : 12, ty);
    ui_style_btn(btn_link, false);
    lv_obj_add_style(btn_link, &style_btn_tactical_pressed, LV_STATE_CHECKED);
    lv_obj_add_flag(btn_link, LV_OBJ_FLAG_CHECKABLE);
    if (link_active()) lv_obj_add_state(btn_link, LV_STATE_CHECKED);
    lv_obj_add_event_cb(btn_link, event_link_toggle, LV_EVENT_CLICKED, NULL);
    lv_obj_t* lbl_link = lv_label_create(btn_link);
    lv_label_set_text(lbl_link, tr(S_COMLINK_LINK));
    lv_obj_set_style_text_font(lbl_link, ui_font(12), 0);
    lv_obj_center(lbl_link);

    lv_obj_t* btn_tx = lv_btn_create(scr_comlink);
    lv_obj_set_size(btn_tx, wide ? col_w : 56, wide ? 80 : 36);
    lv_obj_set_pos(btn_tx, wide ? col_x : 84, wide ? ty + 100 : ty);
    ui_style_btn(btn_tx, false);
    lv_obj_add_style(btn_tx, &style_btn_tactical_pressed, LV_STATE_CHECKED);
    lv_obj_add_flag(btn_tx, LV_OBJ_FLAG_CHECKABLE);
    if (tx_enabled) lv_obj_add_state(btn_tx, LV_STATE_CHECKED);
    lv_obj_add_event_cb(btn_tx, event_tx_toggle, LV_EVENT_CLICKED, NULL);
    lv_obj_t* lbl_tx = lv_label_create(btn_tx);
    lv_label_set_text(lbl_tx, "TX");
    lv_obj_set_style_text_font(lbl_tx, ui_font_latin(12), 0);   // always Latin
    lv_obj_center(lbl_tx);

    // Live status box
    lv_obj_t* box = lv_obj_create(scr_comlink);
    lv_obj_set_size(box, wide ? col_w : 84, wide ? 120 : 36);
    lv_obj_set_pos(box, wide ? col_x : 144, wide ? ty + 200 : ty);
    lv_obj_set_style_bg_color(box, COLOR_PANEL, 0);
    lv_obj_set_style_border_color(box, COLOR_DATA, 0);
    lv_obj_set_style_border_width(box, 1, 0);
    lv_obj_set_style_radius(box, 4, 0);
    lv_obj_set_style_pad_all(box, 2, 0);
    lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    lbl_link_status = lv_label_create(box);
    lv_obj_set_style_text_font(lbl_link_status, ui_font(12), 0);
    lv_obj_set_style_text_color(lbl_link_status, COLOR_DATA, 0);
    lv_obj_center(lbl_link_status);
    link_status_refresh();

    if (!link_status_timer) link_status_timer = lv_timer_create(link_status_timer_cb, 1000, NULL);
    return scr_comlink;
}
