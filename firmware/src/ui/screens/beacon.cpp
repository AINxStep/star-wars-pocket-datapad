#include "../ui_common.h"
#include "aurebesh_font.h"
#include <string.h>

static lv_obj_t* scr_beacon;
static lv_obj_t* lbl_msg;
static lv_obj_t* lbl_morse;
static lv_obj_t* lbl_prog;
static lv_obj_t* btn_tx;
static lv_obj_t* lbl_tx;
static lv_obj_t* btn_strobe;
static lv_timer_t* bcn_timer;

// =========================================================================
// Signal beacon: transmits the message composed in the Aurebesh keyboard as
// morse code — white LED flashes plus an optional full-screen strobe.
// =========================================================================
#define MORSE_UNIT 150           // ms per dit; dah = 3, gaps 1/3/7 units (slow, read by eye at distance)

static const char* MORSE[36] = {
    ".-", "-...", "-.-.", "-..", ".", "..-.", "--.", "....", "..", ".---",
    "-.-", ".-..", "--", "-.", "---", ".--.", "--.-", ".-.", "...", "-",
    "..-", "...-", ".--", "-..-", "-.--", "--..",
    "-----", ".----", "..---", "...--", "....-", ".....", "-....", "--...",
    "---..", "----."
};

// Transmission is a flat step list: bit7 = LED/flash on, bits0-6 = units long
static uint8_t  seq[260];
static uint8_t  seq_chr[260];   // message char each step belongs to
static int      seq_n = 0, seq_i = 0;
static uint32_t step_end = 0;
static bool     tx = false;
static char     tx_msg[24];
static int      cd = 0;              // strobe lead-in countdown (s left), 0 = not counting
static uint32_t cd_next = 0;
static bool     bright_forced = false;
static bool     strobing = false;    // direct panel fill active (bypasses LVGL)

static const char* morse_of(char c) {
    if (c >= 'a' && c <= 'z') c -= 'a' - 'A';
    if (c >= 'A' && c <= 'Z') return MORSE[c - 'A'];
    if (c >= '0' && c <= '9') return MORSE[26 + c - '0'];
    return NULL;
}

// The strobe paints the panel directly (white on, black off) instead of
// toggling an LVGL overlay: a full-screen repaint lands band by band and
// reads as a wipe, which blurs the morse pulse timing
static void strobe_begin(void) {
    if (strobing) return;
    strobing = true;
    board_strobe_begin();
    board_strobe_fill(0x0000);      // dark gap before the first pulse
    app_brightness_override(100);   // real backlight too
    bright_forced = true;
}

static void strobe_apply(bool on) { board_strobe_fill(on ? 0xFFFF : 0x0000); }

static void strobe_end(void) {
    if (!strobing) return;
    strobing = false;
    board_strobe_end();
    if (bright_forced) { bright_forced = false; app_brightness_override(-1); }
    lv_obj_invalidate(lv_scr_act());   // repaint the UI LVGL dropped while held
    lv_obj_invalidate(lv_layer_top());
}

// Countdown digits follow the interface language (Aurebesh numerals included);
// the transmitted morse glyphs themselves always stay Latin
static const lv_font_t* cd_font(void) {
#ifdef HAVE_AUREBESH_FONT
    if (lang_get() == LANG_AUR) return &font_aurebesh_40;   // biggest Aurebesh size generated
#endif
    return &lv_font_montserrat_48;
}

static void beacon_stop(void) {
    tx = false;
    cd = 0;
    strobe_end();
    lv_label_set_text(lbl_tx, tr(S_BCN_SEND));
    lv_obj_set_style_text_font(lbl_morse, ui_font_latin(28), 0);
    lv_label_set_text(lbl_morse, "");
    lv_label_set_text(lbl_prog, "");
}

static void beacon_start(void) {
    const char* m = aurebesh_msg_get();
    if (!m || !*m) m = "SOS";
    strncpy(tx_msg, m, sizeof(tx_msg) - 1);
    tx_msg[sizeof(tx_msg) - 1] = '\0';
    lv_label_set_text(lbl_msg, tx_msg);

    // Flatten the message into on/off steps (dit 1u, dah 3u, intra 1u,
    // char gap +2u on the trailing intra-gap, word gap = one OFF step of 4u)
    seq_n = 0;
    int ci = 0;
    for (const char* p = tx_msg; *p && seq_n < 250; p++, ci++) {
        const char* code = morse_of(*p);
        if (!code) {
            seq[seq_n] = 4; seq_chr[seq_n++] = (uint8_t)ci;
            continue;
        }
        for (const char* s = code; *s && seq_n < 252; s++) {
            seq[seq_n] = 0x80 | (*s == '-' ? 3 : 1); seq_chr[seq_n++] = (uint8_t)ci;
            seq[seq_n] = 1;                            seq_chr[seq_n++] = (uint8_t)ci;
        }
        seq[seq_n - 1] += 2;   // char gap: last OFF step goes 1 -> 3 units
    }

    seq_i = 0;
    lv_label_set_text(lbl_tx, tr(S_BCN_STOP));
    if (lv_obj_has_state(btn_strobe, LV_STATE_CHECKED)) {
        // 5 s on-screen lead-in before the strobe fires
        cd = 5;
        cd_next = lv_tick_get() + 1000;
        lv_obj_set_style_text_font(lbl_morse, cd_font(), 0);   // bigger countdown digits
        lv_label_set_text_fmt(lbl_morse, "%d", cd);
    } else {
        step_end = lv_tick_get();
        tx = true;
        sfx_comlink_chirp();
    }
}

static void bcn_timer_cb(lv_timer_t* t) {
    if (lv_scr_act() != scr_beacon) {
        if (tx || cd) beacon_stop();   // leaving the screen cuts the signal
        return;
    }
    uint32_t now = lv_tick_get();
    if (cd > 0) {
        if ((int32_t)(now - cd_next) < 0) return;
        cd_next = now + 1000;
        if (--cd == 1) sfx_click();            // beep one second before firing
        if (cd > 0) {
            lv_label_set_text_fmt(lbl_morse, "%d", cd);
            return;
        }
        lv_obj_set_style_text_font(lbl_morse, ui_font_latin(28), 0);   // back to the morse font
        if (lv_obj_has_state(btn_strobe, LV_STATE_CHECKED)) strobe_begin();
        step_end = now;
        tx = true;
        sfx_comlink_chirp();
    }
    if (!tx) return;
    if (strobing && !lv_obj_has_state(btn_strobe, LV_STATE_CHECKED)) strobe_end();
    if ((int32_t)(now - step_end) < 0) return;

    if (seq_i >= seq_n) { beacon_stop(); return; }

    uint8_t st = seq[seq_i];
    int dur = (st & 0x7f) * MORSE_UNIT;
    int ci = seq_chr[seq_i];
    if (st & 0x80) {
        hw_led_pulse(true, true, true, dur);   // white LED
        if (lv_obj_has_state(btn_strobe, LV_STATE_CHECKED)) {
            strobe_begin();
            strobe_apply(true);
        }
    } else if (strobing) {
        strobe_apply(false);
    }

    const char* code = morse_of(tx_msg[ci]);
    lv_label_set_text_fmt(lbl_morse, "%c   %s", tx_msg[ci], code ? code : "");
    lv_label_set_text_fmt(lbl_prog, "%d/%d", ci + 1, (int)strlen(tx_msg));

    step_end = now + dur;
    seq_i++;
}

static void event_tx(lv_event_t* e) {
    if (tx || cd > 0) { beacon_stop(); sfx_click(); }
    else beacon_start();
}

static void event_strobe(lv_event_t* e) {
    sfx_click();
}

// Screens are built once at boot; refresh the card with whatever the Aurebesh
// translator holds right now every time this screen comes up.
static void refresh_msg_cb(lv_event_t* e) {
    const char* m = aurebesh_msg_get();
    lv_label_set_text(lbl_msg, (m && *m) ? m : "SOS");
}

lv_obj_t* screen_beacon_create(void) {
    scr_beacon = lv_obj_create(NULL);
    lv_obj_add_style(scr_beacon, &style_base, 0);
    lv_obj_clear_flag(scr_beacon, LV_OBJ_FLAG_SCROLLABLE);
    ui_attach_top_bar(scr_beacon, tr(S_BCN_TITLE), true);
    lv_obj_add_event_cb(scr_beacon, refresh_msg_cb, LV_EVENT_SCREEN_LOAD_START, NULL);

    const bool wide = UI_WIDE;

    // Message card
    lv_obj_t* card = lv_obj_create(scr_beacon);
    lv_obj_set_size(card, wide ? 560 : 216, wide ? 84 : 52);
    lv_obj_align(card, LV_ALIGN_TOP_MID, 0, wide ? TOP_BAR_H + 24 : 50);
    lv_obj_add_style(card, &style_card, 0);
    lv_obj_set_style_pad_all(card, 4, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    lbl_msg = lv_label_create(card);
    lv_label_set_text(lbl_msg, aurebesh_msg_get()[0] ? aurebesh_msg_get() : "SOS");
    lv_obj_set_style_text_font(lbl_msg, ui_font(16), 0);
    lv_obj_set_style_text_color(lbl_msg, COLOR_ACCENT, 0);
    // Long messages marquee horizontally instead of wrapping out of the card
    lv_obj_set_width(lbl_msg, wide ? 540 : 200);
    lv_label_set_long_mode(lbl_msg, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_center(lbl_msg);

    // Current symbol, big, and the progress counter
    lbl_morse = lv_label_create(scr_beacon);
    lv_label_set_text(lbl_morse, "");
    lv_obj_set_style_text_font(lbl_morse, ui_font_latin(28), 0);   // morse stays Latin
    lv_obj_set_style_text_color(lbl_morse, COLOR_HILITE, 0);
    lv_obj_set_style_text_align(lbl_morse, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(lbl_morse, LV_ALIGN_TOP_MID, 0, wide ? TOP_BAR_H + 150 : 128);

    lbl_prog = lv_label_create(scr_beacon);
    lv_label_set_text(lbl_prog, "");
    lv_obj_set_style_text_font(lbl_prog, ui_font(12), 0);
    lv_obj_set_style_text_color(lbl_prog, COLOR_DATA, 0);
    lv_obj_align(lbl_prog, LV_ALIGN_TOP_MID, 0, wide ? TOP_BAR_H + 230 : 166);

    // Controls: transmit toggle + strobe on/off
    btn_tx = lv_btn_create(scr_beacon);
    lv_obj_set_size(btn_tx, wide ? 280 : 120, wide ? 72 : 42);
    if (wide) lv_obj_align(btn_tx, LV_ALIGN_BOTTOM_MID, -150, -72);
    else      lv_obj_align(btn_tx, LV_ALIGN_BOTTOM_LEFT, 12, -58);
    ui_style_btn(btn_tx, false);
    lv_obj_add_event_cb(btn_tx, event_tx, LV_EVENT_CLICKED, NULL);
    lbl_tx = lv_label_create(btn_tx);
    lv_label_set_text(lbl_tx, tr(S_BCN_SEND));
    lv_obj_center(lbl_tx);

    btn_strobe = lv_btn_create(scr_beacon);
    lv_obj_set_size(btn_strobe, wide ? 250 : 96, wide ? 72 : 42);
    if (wide) lv_obj_align(btn_strobe, LV_ALIGN_BOTTOM_MID, 150, -72);
    else      lv_obj_align(btn_strobe, LV_ALIGN_BOTTOM_RIGHT, -12, -58);
    ui_style_btn(btn_strobe, false);
    lv_obj_add_style(btn_strobe, &style_btn_tactical_pressed, LV_STATE_CHECKED);
    lv_obj_add_flag(btn_strobe, LV_OBJ_FLAG_CHECKABLE);
    lv_obj_add_state(btn_strobe, LV_STATE_CHECKED);      // strobe on by default
    lv_obj_add_event_cb(btn_strobe, event_strobe, LV_EVENT_CLICKED, NULL);
    lv_obj_t* ls = lv_label_create(btn_strobe);
    lv_label_set_text(ls, tr(S_BCN_STROBE));
    lv_obj_set_style_text_font(ls, ui_font(12), 0);
    lv_obj_center(ls);

    lv_obj_t* hint = lv_label_create(scr_beacon);
    lv_label_set_text(hint, tr(S_BCN_HINT));
    lv_obj_set_style_text_font(hint, ui_font(12), 0);
    lv_obj_set_style_text_color(hint, COLOR_DATA, 0);
    lv_obj_set_style_text_align(hint, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, wide ? -24 : -8);

    if (!bcn_timer) bcn_timer = lv_timer_create(bcn_timer_cb, 30, NULL);
    return scr_beacon;
}
