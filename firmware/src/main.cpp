#include <Arduino.h>
#include <lvgl.h>
#include "board.h"
#include "hardware.h"
#include "settings.h"
#include "starwars_audio.h"
#include "link.h"
#include "ui/ui.h"

// =========================================================================
// Auto-brightness (LDR, where the board has one) and display sleep
// settings().brightness == 0 means AUTO: the backlight follows the LDR.
// =========================================================================
// Overridable at build time for bench tests (PLATFORMIO_BUILD_FLAGS="-D ...").
#ifndef SLEEP_TIMEOUT_MS
#define SLEEP_TIMEOUT_MS      180000    // screen on 3 min, then backlight + panel off
#endif
#ifndef DEEP_SLEEP_TIMEOUT_MS
#define DEEP_SLEEP_TIMEOUT_MS 3600000   // 3 min lit + 57 min dark = 60 min total
#endif

static int auto_brightness_pct = BOARD_HAS_LDR ? 60 : 100;   // last level derived from the LDR (full without one)
static int bright_override = -1;      // >=0 forces the backlight level (beacon strobe)
static uint32_t last_ldr_read = 0;
static int ldr_ema = -1;
static bool display_asleep = false;
static bool wake_swallow = false;       // the touch that wakes does not click

// Takes the stored setting (0 = AUTO) and drives the real backlight level
void app_apply_brightness(uint8_t pct) {
    uint16_t eff = (pct == 0) ? (uint16_t)auto_brightness_pct : pct;
    board_set_backlight(eff);
}

// Forces a backlight level (>=0) suspending AUTO updates and display sleep;
// -1 releases and reapplies the user's configured brightness
void app_brightness_override(int pct) {
    bright_override = pct;
    if (pct < 0) app_apply_brightness(settings().brightness);
    else board_set_backlight(pct);
}

static void app_light_update() {
    if (!BOARD_HAS_LDR || settings().brightness != 0 || display_asleep || bright_override >= 0) return;
    uint32_t now = millis();
    if (now - last_ldr_read < 250) return;
    last_ldr_read = now;

    int raw = board_ldr_raw();
    if (ldr_ema < 0) ldr_ema = raw;
    ldr_ema = (ldr_ema * 4 + raw) / 5;
    int pct = map(ldr_ema, 0, 4095, 15, 100);   // never below 15%: stays readable
    if (abs(pct - auto_brightness_pct) >= 2) {
        auto_brightness_pct = pct;
        board_set_backlight(pct);
        Serial.printf("[LDR] raw %d -> %d%%\n", raw, pct);
    }
}

// Touchpad Read Callback for LVGL
static void my_touchpad_read(lv_indev_drv_t* indev_driver, lv_indev_data_t* data) {
    static bool was_pressed = false;
    TouchRaw& raw = touch_raw();
    int16_t sx, sy, rx, ry;
    if (board_touch_read(&sx, &sy, &rx, &ry)) {
        if (display_asleep) {           // first touch only wakes the display
            display_asleep = false;
            wake_swallow = true;
            board_display_power(true);
            app_apply_brightness(settings().brightness);
            lv_disp_trig_activity(NULL);
            lv_obj_invalidate(lv_scr_act());   // repaint: a torn frame would persist otherwise
        }
        if (wake_swallow) {
            data->state = LV_INDEV_STATE_REL;
            return;
        }
        data->state = LV_INDEV_STATE_PR;

        raw.x = rx;
        raw.y = ry;
        raw.pressed = true;
        raw.seq++;

        data->point.x = sx;
        data->point.y = sy;

        if (!was_pressed) hw_led_pulse(false, true, false, 80); // Green flash on touch-down
        was_pressed = true;
    } else {
        data->state = LV_INDEV_STATE_REL;
        was_pressed = false;
        wake_swallow = false;
        raw.pressed = false;
        raw.seq++;
    }
}

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println("\n==========================================");
    Serial.println("  STAR WARS REBEL DATAPAD MK-IV (" BOARD_NAME ")");
    Serial.println("==========================================");

    // Load persisted settings (brightness, volume, touch calibration)
    settings_load();

    // Initialize LVGL Graphics Engine first: its memory pool is one big
    // malloc() and must claim a contiguous block while the heap is still
    // clean (the audio clips loaded right after would fragment it)
    lv_init();

    // Wi-Fi + ESP-NOW must init before audio_init: the radio driver needs
    // large contiguous heap blocks and phy_init aborts if they are missing
    link_wifi_init();

    // Initialize Onboard Hardware
    board_init_io();
    audio_set_volume(settings().volume);
    audio_init(board_sd_spi());   // renders built-ins, loads /sounds/*.wav from SD, starts playback

    // Display (panel + touch controller) and its LVGL driver
    board_init_display();
    audio_start_output();   // S3: codec and I2S need the display's I2C bus
    app_apply_brightness(settings().brightness);

    // Initialize Touch Input Driver in LVGL
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = my_touchpad_read;
    lv_indev_drv_register(&indev_drv);

    // Initialize RebelOS Tactical UI
    ui_init();

    // Play startup chirp with blue LED
    hw_led_pulse(false, false, true, sfx_comlink_chirp());
    Serial.printf("[MEM] heap free %u, largest block %u\n", (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxAllocHeap());

    Serial.println("[OK] System Ready. Touch & UI Active.");
}

void loop() {
    lv_timer_handler();
    hw_led_update();
    app_light_update();

    // TinyUSB: the console only exists while a host holds the port open, so
    // boot prints are dropped. Re-emit the audio load report on first attach.
    static bool audio_log_done = false;
    if (!audio_log_done && Serial) {
        audio_log_done = true;
        audio_print_load_report();
    }
#if BOARD_HAS_BATT_ADC
    // Battery calibration aid: periodic pin/Vbat/% readout while a host holds
    // the USB console open. Compare "vbat" with a multimeter to fix the divider.
    static uint32_t batt_log_next = 0;
    if (Serial && millis() > batt_log_next) {
        batt_log_next = millis() + 10000;
        board_batt_debug();
    }
#endif

    // Power saving: screen off after 3 min without touch (any touch wakes;
    // the waking press itself is swallowed and never reaches the UI).
    // The panel sleeps with it, so the dark window costs no SPI traffic.
    if (settings().sleep_en && !display_asleep && bright_override < 0 &&
        lv_disp_get_inactive_time(NULL) > SLEEP_TIMEOUT_MS) {
        board_set_backlight(0);
        board_display_power(false);
        display_asleep = true;
    }

#if BOARD_HAS_DEEP_SLEEP
    // 60 min total without touch: drop the SoC into deep sleep. Touching the
    // screen reboots the board (splash -> Home), like a power switch would.
    if (settings().sleep_en && bright_override < 0 &&
        lv_disp_get_inactive_time(NULL) > DEEP_SLEEP_TIMEOUT_MS) {
        board_enter_deep_sleep();   // never returns
    }
#endif

    // Hold BOOT for 2 s (after startup) to launch touch calibration,
    // useful if a bad calibration makes the touch screen unusable
    static uint32_t boot_down_at = 0;
    static bool boot_fired = false;
    if (BOARD_HAS_RESISTIVE && board_boot_pressed()) {
        if (boot_down_at == 0) boot_down_at = millis();
        if (!boot_fired && millis() - boot_down_at > 2000) {
            boot_fired = true;  // once per press
            ui_goto_calibration();
        }
    } else {
        boot_down_at = 0;
        boot_fired = false;
    }

    delay(5);
}
