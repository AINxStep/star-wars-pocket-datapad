#pragma once
#include <Arduino.h>
#include <SPI.h>
#include <lvgl.h>

// =========================================================================
// Board abstraction. The UI, audio engine and link code are board-agnostic;
// everything that touches pins, panels or touch controllers lives behind
// this header, implemented once per board:
//   BOARD_ES3C28P -> src/board_es3c28p.cpp (ES3C28P, 240x320 SPI +
//                                          ILI9341V + FT6336G, ES8311 audio)
//   BOARD_SIM     -> simulator/src/board_sim.cpp (web browser layer)
// The board is selected by a build flag in platformio.ini.
// =========================================================================

#if defined(BOARD_ES3C28P)
  #define BOARD_NAME            "ES3C28P"
  #define BOARD_HAS_LDR         0
  #define BOARD_HAS_RGB_LED     1     // single WS2812 on GPIO42
  #define BOARD_HAS_BATT_ADC    1     // TP4054 charger + divider on GPIO9
  #define BOARD_HAS_RESISTIVE   0     // capacitive FT6336G, factory calibrated
  #define BOARD_BRIGHTNESS_LIVE 1     // real LEDC backlight
  #define BOARD_HAS_DEEP_SLEEP  1     // FT6336G INT on GPIO17 (RTC GPIO) wakes it
#elif defined(BOARD_SIM)
  // Web simulator (simulator/): the page picks the resolution, so both layouts run
  #define BOARD_NAME            "SIM"
  #define BOARD_HAS_LDR         0
  #define BOARD_HAS_RGB_LED     1     // drawn as an indicator on the page
  #define BOARD_HAS_BATT_ADC    0
  #define BOARD_HAS_RESISTIVE   0     // the pointer needs no calibration
  #define BOARD_BRIGHTNESS_LIVE 1
  #define BOARD_HAS_DEEP_SLEEP  0
#else
  #error "Define BOARD_ES3C28P or BOARD_SIM (see platformio.ini)"
#endif

// Pins, clocks, panel, touch controller, framebuffers. Registers the LVGL
// display driver (the pointer driver is registered by main.cpp on top of
// board_touch_read). Call after lv_init() and link_wifi_init().
void board_init_display();

// Pins that do not depend on the panel (LED, ADC, speaker pin). Safe to call
// before audio_init().
void board_init_io();

// SPI bus the audio engine may borrow at boot to read /sounds from the SD card
SPIClass& board_sd_spi();

void board_set_backlight(uint8_t pct);          // 0..100

// Beacon strobe: instant flat fills that bypass LVGL entirely. A full-screen
// LVGL repaint renders band by band and reads as a "wipe", blurring the morse
// pulse timing; these push one flat frame straight to the panel instead.
// begin() suspends LVGL flushes, fill() paints an RGB565 color, end() resumes
// LVGL — the caller must invalidate the screen afterwards to repaint the UI.
void board_strobe_begin(void);
void board_strobe_fill(uint16_t rgb565);
void board_strobe_end(void);

// Raw LDR reading (0..4095) or -1 when the board has none
int board_ldr_raw();

// LiPo percentage (0..100) or -1 when the board cannot measure it
int board_batt_pct();

// Prints raw pin mV / computed Vbat / percentage over the USB console, for
// calibrating the divider ratio against a multimeter. Boards with
// BOARD_HAS_BATT_ADC only.
void board_batt_debug(void);

// RGB status LED (true = lit); no-op on boards without one
void board_set_rgb(bool r, bool g, bool b);

bool board_boot_pressed();

// Polls the touch controller. Returns true while the screen is pressed and
// fills sx/sy with UI coordinates (0..W-1, 0..H-1). raw_x/raw_y receive the
// controller's native reading (used by the resistive calibration screen).
bool board_touch_read(int16_t* sx, int16_t* sy, int16_t* raw_x, int16_t* raw_y);

// Flash writes (NVS) disable the CPU cache. Boards whose display DMA reads
// its framebuffer through that cache (the S3 RGB panel in PSRAM) must stop
// the panel between these two calls. No-ops elsewhere.
void board_storage_begin();
void board_storage_end();

// Keeps an undimmed copy of the screen so brightness can follow a slider
// live. Keeping it costs PSRAM bandwidth on every frame, so it is only
// active while the Settings screen is visible.
void board_dim_shadow(bool on);

// Panel power state, separate from the backlight: off() sleeps the panel and
// drops LVGL flushes (no SPI traffic while the screen is dark anyway), on()
// wakes it. Callers must invalidate the screen after on(). No-op on boards
// where it isn't worth it.
void board_display_power(bool on);

// Puts the SoC into deep sleep, arming the touch wake source. Never returns:
// waking is a full reset. ES3C28P only (BOARD_HAS_DEEP_SLEEP).
void board_enter_deep_sleep(void);
