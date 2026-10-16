#include "../ui_common.h"
#include "aurebesh_font.h"
#include <string.h>

static lv_obj_t* scr_aurebesh;

// =========================================================================
// 5. Aurebesh Alphabet Screen
// =========================================================================
#ifdef HAVE_AUREBESH_FONT
  #define AU_KEY_FONT  (UI_WIDE ? &font_aurebesh_28 : &font_aurebesh_22)
  #define AU_BIG_FONT  (&font_aurebesh_40)
#else
  #define AU_KEY_FONT  (UI_WIDE ? &lv_font_montserrat_24 : &lv_font_montserrat_16)   // fallback: Latin letters
  #define AU_BIG_FONT  (UI_WIDE ? &lv_font_montserrat_40 : &lv_font_montserrat_28)
#endif

#define AU_KEY_SPACE   26
#define AU_KEY_DELETE  27
#define AU_MSG_MAX     22

static const char* aurebesh_names[26] = {
    "AUREK", "BESH", "CRESH", "DORN", "ESK", "FORN", "GREK", "HERF", "ISK",
    "JENTH", "KRILL", "LETH", "MERN", "NERN", "OSK", "PETH", "QEK", "RESH",
    "SENTH", "TRILL", "USK", "VEV", "WESK", "XESH", "YIRT", "ZEREK"
};

static lv_obj_t* lbl_au_glyph;   // selected letter in Aurebesh
static lv_obj_t* lbl_au_latin;   // selected letter in Latin
static lv_obj_t* lbl_au_name;    // letter name
static lv_obj_t* lbl_au_msg;     // composed message in Aurebesh
static lv_obj_t* msg_strip;      // its scrollable container
static char au_msg[AU_MSG_MAX + 1] = "";

static void au_refresh_msg(void) {
    lv_label_set_text(lbl_au_msg, au_msg[0] ? au_msg : " ");
    // Keep the newest line visible: the wrapped label outgrows the strip, so
    // scroll it to the bottom on every change (the user can still drag up).
    lv_obj_scroll_to_y(msg_strip, LV_COORD_MAX, LV_ANIM_ON);
}

static void event_au_key(lv_event_t* e) {
    int key = (int)(uintptr_t)lv_event_get_user_data(e);
    size_t len = strlen(au_msg);
    sfx_click();

    if (key == AU_KEY_DELETE) {
        if (len) au_msg[len - 1] = '\0';
        au_refresh_msg();
        return;
    }

    char c = (key == AU_KEY_SPACE) ? ' ' : (char)('A' + key);
    if (len >= AU_MSG_MAX) {            // full: drop the oldest character
        memmove(au_msg, au_msg + 1, len - 1);
        len--;
    }
    au_msg[len] = c;
    au_msg[len + 1] = '\0';
    au_refresh_msg();

    if (key < 26) {
        char latin[2] = { c, '\0' };
        lv_label_set_text(lbl_au_glyph, latin);
        lv_label_set_text(lbl_au_latin, latin);
        lv_label_set_text(lbl_au_name, aurebesh_names[key]);
    }
}

static void event_au_clear(lv_event_t* e) {
    au_msg[0] = '\0';
    au_refresh_msg();
}

// The composed line doubles as the beacon's morse message
const char* aurebesh_msg_get(void) {
    return au_msg;
}

lv_obj_t* screen_aurebesh_create(void) {
    const bool wide = UI_WIDE;
    scr_aurebesh = lv_obj_create(NULL);
    lv_obj_add_style(scr_aurebesh, &style_base, 0);
    lv_obj_clear_flag(scr_aurebesh, LV_OBJ_FLAG_SCROLLABLE);
    ui_attach_top_bar(scr_aurebesh, tr(S_AUREBESH_TITLE), true);

    // Wide layout: card and message on the left, keyboard on the right
    const int left_x = 24, left_w = 280;

    // Selected letter card: big glyph | Latin letter + name
    lv_obj_t* card = lv_obj_create(scr_aurebesh);
    lv_obj_set_size(card, wide ? left_w : 216, wide ? 130 : 60);
    if (wide) lv_obj_set_pos(card, left_x, TOP_BAR_H + 16);
    else      lv_obj_align(card, LV_ALIGN_TOP_MID, 0, 48);
    lv_obj_add_style(card, &style_card, 0);
    lv_obj_set_style_pad_all(card, 4, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    lbl_au_glyph = lv_label_create(card);
    lv_label_set_text(lbl_au_glyph, "A");
    lv_obj_set_style_text_font(lbl_au_glyph, AU_BIG_FONT, 0);
    lv_obj_set_style_text_color(lbl_au_glyph, COLOR_DATA, 0);
    lv_obj_align(lbl_au_glyph, LV_ALIGN_LEFT_MID, wide ? 18 : 14, 0);

    lbl_au_latin = lv_label_create(card);
    lv_label_set_text(lbl_au_latin, "A");
    lv_obj_set_style_text_font(lbl_au_latin, ui_font_latin(28), 0);
    lv_obj_set_style_text_color(lbl_au_latin, COLOR_ACCENT, 0);
    lv_obj_align(lbl_au_latin, LV_ALIGN_LEFT_MID, wide ? 96 : 82, -2);

    lbl_au_name = lv_label_create(card);
    lv_label_set_text(lbl_au_name, aurebesh_names[0]);
    lv_obj_set_style_text_font(lbl_au_name, ui_font_latin(16), 0);
    lv_obj_set_style_text_color(lbl_au_name, COLOR_TEXT, 0);
    lv_obj_align(lbl_au_name, LV_ALIGN_LEFT_MID, wide ? 150 : 118, -2);

    // Message line (tap letters to compose, DEL erases, hold DEL to clear)
    lv_obj_t* strip = msg_strip = lv_obj_create(scr_aurebesh);
    lv_obj_set_size(strip, wide ? left_w : 216, wide ? 200 : 54);
    if (wide) lv_obj_set_pos(strip, left_x, TOP_BAR_H + 16 + 130 + 16);
    else      lv_obj_align(strip, LV_ALIGN_TOP_MID, 0, 112);
    lv_obj_set_style_bg_color(strip, COLOR_BG, 0);
    lv_obj_set_style_border_color(strip, COLOR_ACCENT, 0);
    lv_obj_set_style_border_width(strip, 1, 0);
    lv_obj_set_style_radius(strip, 4, 0);
    lv_obj_set_style_pad_all(strip, wide ? 8 : 4, 0);
    lv_obj_set_scroll_dir(strip, LV_DIR_VER);   // wraps taller than the box: scroll instead of clipping

    lbl_au_msg = lv_label_create(strip);
    lv_obj_set_width(lbl_au_msg, wide ? left_w - 20 : 206);
    lv_label_set_long_mode(lbl_au_msg, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(lbl_au_msg, AU_KEY_FONT, 0);
    lv_obj_set_style_text_color(lbl_au_msg, COLOR_TEXT, 0);
    au_msg[0] = '\0';
    au_refresh_msg();

    // Keyboard: 26 letters + space + delete, 7 columns x 4 rows
    const int cols = 7;
    const int kw = wide ? 62 : 29, kh = wide ? 76 : 32, gap = wide ? 6 : 2, vgap = wide ? 8 : 3;
    const int kx0 = wide ? 324 : 12, ky0 = wide ? TOP_BAR_H + 16 : 172;
    for (int k = 0; k < 28; k++) {
        lv_obj_t* btn = lv_btn_create(scr_aurebesh);
        lv_obj_set_size(btn, kw, kh);
        lv_obj_set_pos(btn, kx0 + (k % cols) * (kw + gap), ky0 + (k / cols) * (kh + vgap));
        ui_style_btn(btn, k == AU_KEY_DELETE);
        lv_obj_set_style_border_width(btn, 1, 0);
        lv_obj_set_style_pad_all(btn, 0, 0);
        // CLICKED: a firm press longer than the long-press time still works.
        // DELETE is the exception: it also clears on LONG_PRESSED, and CLICKED
        // would fire again on release — SHORT_CLICKED keeps them exclusive.
        lv_event_code_t ev = (k == AU_KEY_DELETE) ? LV_EVENT_SHORT_CLICKED : LV_EVENT_CLICKED;
        lv_obj_add_event_cb(btn, event_au_key, ev, (void*)(uintptr_t)k);
        if (k == AU_KEY_DELETE) lv_obj_add_event_cb(btn, event_au_clear, LV_EVENT_LONG_PRESSED, NULL);

        lv_obj_t* lbl = lv_label_create(btn);
        lv_obj_center(lbl);
        if (k == AU_KEY_SPACE) {
            lv_label_set_text(lbl, tr(S_AUREBESH_SPACE));
            lv_obj_set_style_text_font(lbl, ui_font_latin(12), 0);
        } else if (k == AU_KEY_DELETE) {
            lv_label_set_text(lbl, LV_SYMBOL_BACKSPACE);
            if (wide) lv_obj_set_style_text_font(lbl, ui_font_latin(16), 0);
        } else {
            char latin[2] = { (char)('A' + k), '\0' };
            lv_label_set_text(lbl, latin);
            lv_obj_set_style_text_font(lbl, AU_KEY_FONT, 0);
        }
    }
    return scr_aurebesh;
}
