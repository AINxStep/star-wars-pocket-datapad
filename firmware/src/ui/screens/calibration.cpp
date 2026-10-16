#include "../ui_common.h"
#include <math.h>

static lv_obj_t* scr_calib;
static lv_obj_t* calib_target;
static lv_obj_t* lbl_calib;
static lv_timer_t* calib_timer;
static lv_obj_t* calib_return_scr;
static int calib_step = 0;           // 0..3 = waiting for target, 4 = done
static bool calib_pressing = false;
static uint32_t calib_last_seq = 0;
static int32_t calib_sum_x, calib_sum_y, calib_n;
static int calib_raw_x[4], calib_raw_y[4];

// Calibration targets (screen px), inset from the corners
#define CALIB_INSET 24
static const lv_point_t calib_pts[4] = {
    {CALIB_INSET, CALIB_INSET}, {240 - CALIB_INSET, CALIB_INSET},
    {CALIB_INSET, 320 - CALIB_INSET}, {240 - CALIB_INSET, 320 - CALIB_INSET}
};

// =========================================================================
// 7. Touch Calibration Screen (4-point)
// =========================================================================
static void calib_leave(void) {
    lv_timer_pause(calib_timer);
    lv_obj_t* back = (calib_return_scr && calib_return_scr != ui_screen(SCR_SPLASH) && calib_return_scr != scr_calib)
                         ? calib_return_scr : ui_screen(SCR_HOME);
    lv_scr_load_anim(back, LV_SCR_LOAD_ANIM_FADE_ON, 150, 0, false);
}

// Derives the raw->screen ranges from the four corner samples.
// Returns false if the samples are inconsistent (e.g. wrong targets touched).
static bool calib_compute(void) {
    float rx_l = (calib_raw_x[0] + calib_raw_x[2]) / 2.0f;
    float rx_r = (calib_raw_x[1] + calib_raw_x[3]) / 2.0f;
    float ry_t = (calib_raw_y[0] + calib_raw_y[1]) / 2.0f;
    float ry_b = (calib_raw_y[2] + calib_raw_y[3]) / 2.0f;

    float span_x = rx_r - rx_l;
    float span_y = ry_b - ry_t;
    if (fabsf(span_x) < 800 || fabsf(span_y) < 800) return false;
    // Both samples on the same edge must agree
    if (fabsf(calib_raw_x[0] - calib_raw_x[2]) > fabsf(span_x) * 0.35f) return false;
    if (fabsf(calib_raw_x[1] - calib_raw_x[3]) > fabsf(span_x) * 0.35f) return false;
    if (fabsf(calib_raw_y[0] - calib_raw_y[1]) > fabsf(span_y) * 0.35f) return false;
    if (fabsf(calib_raw_y[2] - calib_raw_y[3]) > fabsf(span_y) * 0.35f) return false;

    float per_px_x = span_x / (240 - 2 * CALIB_INSET);
    float per_px_y = span_y / (320 - 2 * CALIB_INSET);

    Settings& cfg = settings();
    cfg.touch_x_min = (int16_t)(rx_l - CALIB_INSET * per_px_x);
    cfg.touch_x_max = (int16_t)(rx_l + (240 - CALIB_INSET) * per_px_x);
    cfg.touch_y_min = (int16_t)(ry_t - CALIB_INSET * per_px_y);
    cfg.touch_y_max = (int16_t)(ry_t + (320 - CALIB_INSET) * per_px_y);
    settings_save();
    return true;
}

static void calib_timer_cb(lv_timer_t* t) {
    static uint32_t done_at = 0;

    if (calib_step >= 4) {
        if (lv_tick_elaps(done_at) > 1000) calib_leave();
        return;
    }

    TouchRaw& raw = touch_raw();
    if (raw.seq == calib_last_seq) return;
    calib_last_seq = raw.seq;

    if (raw.pressed) {
        if (!calib_pressing) {          // first sample of a touch is noisy: drop it
            calib_pressing = true;
            calib_sum_x = calib_sum_y = calib_n = 0;
        } else {
            calib_sum_x += raw.x;
            calib_sum_y += raw.y;
            calib_n++;
        }
        return;
    }

    if (!calib_pressing) return;
    calib_pressing = false;
    if (calib_n < 3) return;            // too short, ask again

    calib_raw_x[calib_step] = calib_sum_x / calib_n;
    calib_raw_y[calib_step] = calib_sum_y / calib_n;
    calib_step++;
    sfx_click();

    if (calib_step < 4) {
        lv_obj_set_pos(calib_target, calib_pts[calib_step].x - 14, calib_pts[calib_step].y - 14);
        lv_label_set_text_fmt(lbl_calib, tr(S_CAL_PROMPT_FMT), calib_step + 1);
        return;
    }

    done_at = lv_tick_get();
    if (calib_compute()) {
        lv_label_set_text(lbl_calib, tr(S_CAL_SAVED));
        hw_led_pulse(false, true, false, 600);
    } else {
        lv_label_set_text(lbl_calib, tr(S_CAL_ERROR));
        hw_led_pulse(true, false, false, 600);
    }
}

static void event_calib_cancel(lv_event_t* e) {
    if (calib_step < 4) calib_leave();
}

lv_obj_t* screen_calibration_create(void) {
    scr_calib = lv_obj_create(NULL);
    lv_obj_add_style(scr_calib, &style_base, 0);
    lv_obj_clear_flag(scr_calib, LV_OBJ_FLAG_SCROLLABLE);

    lbl_calib = lv_label_create(scr_calib);
    lv_label_set_text(lbl_calib, "");
    lv_obj_set_style_text_align(lbl_calib, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(lbl_calib, COLOR_ACCENT, 0);
    lv_obj_align(lbl_calib, LV_ALIGN_CENTER, 0, -40);

    lv_obj_t* cancel = lv_btn_create(scr_calib);
    lv_obj_set_size(cancel, 120, 40);
    lv_obj_align(cancel, LV_ALIGN_CENTER, 0, 30);
    ui_style_btn(cancel, true);
    lv_obj_add_event_cb(cancel, event_calib_cancel, LV_EVENT_CLICKED, NULL);
    lv_obj_t* lbl = lv_label_create(cancel);
    lv_label_set_text(lbl, tr(S_CAL_CANCEL));
    lv_obj_center(lbl);

    // Crosshair target: ring + two thin bars
    calib_target = lv_obj_create(scr_calib);
    lv_obj_set_size(calib_target, 28, 28);
    lv_obj_clear_flag(calib_target, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_opa(calib_target, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_color(calib_target, COLOR_ACCENT, 0);
    lv_obj_set_style_border_width(calib_target, 2, 0);
    lv_obj_set_style_radius(calib_target, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_pad_all(calib_target, 0, 0);

    for (int k = 0; k < 2; k++) {
        lv_obj_t* bar = lv_obj_create(calib_target);
        lv_obj_set_size(bar, k == 0 ? 28 : 2, k == 0 ? 2 : 28);
        lv_obj_center(bar);
        lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_bg_color(bar, COLOR_ACCENT, 0);
        lv_obj_set_style_border_width(bar, 0, 0);
        lv_obj_set_style_radius(bar, 0, 0);
    }

    if (!calib_timer) {   // survives screen rebuilds
        calib_timer = lv_timer_create(calib_timer_cb, 20, NULL);
        lv_timer_pause(calib_timer);
    }
    return scr_calib;
}


// Resets the sequence and remembers where to return; the caller loads the screen
void ui_calibration_begin(void) {
    calib_return_scr = lv_scr_act();
    calib_step = 0;
    calib_pressing = false;
    lv_obj_set_pos(calib_target, calib_pts[0].x - 14, calib_pts[0].y - 14);
    lv_label_set_text_fmt(lbl_calib, tr(S_CAL_PROMPT_FMT), 1);
    lv_timer_resume(calib_timer);
}
