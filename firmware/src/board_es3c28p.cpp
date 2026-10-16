#ifdef BOARD_ES3C28P

#include "board.h"
#include "es3c28p_config.h"
#include "starwars_audio.h"
#include <Wire.h>
#include <esp_heap_caps.h>
#include <esp_sleep.h>
#include <driver/gpio.h>

// =========================================================================
// LCDWIKI ES3C28P: 240x320 ILI9341V IPS on SPI3 (LovyanGFX), FT6336G
// capacitive touch + ES8311 codec sharing I2C port 0 (SDA 16, SCL 15),
// single WS2812 on GPIO42, battery divider on GPIO9, microSD on SDIO.
// =========================================================================
static const uint16_t screenWidth  = 240;
static const uint16_t screenHeight = 320;

// Panel orientation: rotation that puts the USB-C connector at the bottom.
// Verified on hardware: 0 shows the UI upside down; 2 is upright.
#define DISPLAY_ROTATION  2

// Touch-to-screen mapping, applied to the FT6336G native reading. Flip these
// if the pointer is mirrored after checking the display rotation above.
// Verified: at rotation 2 the sensor reads match the UI with no flips.
#define TOUCH_SWAP_XY   0
#define TOUCH_MIRROR_X  0
#define TOUCH_MIRROR_Y  0

// ---- Pins (LCDWIKI IO allocation table) ----
#define TFT_CS       10
#define TFT_DC       46
#define TFT_SCK      12
#define TFT_MOSI     11
#define TFT_MISO     13
#define TFT_BL       45     // active high

#define TP_SDA       16
#define TP_SCL       15
#define TP_RST       18
#define TP_INT       17
#define TP_ADDR      0x38   // FT6336G

#define RGB_LED_PIN  42     // single-wire WS2812
#define BATT_PIN     9      // ADC1_CH8: battery divider
#define BOOT_BTN     0

static LGFX tft;

static SPIClass dummy_spi(FSPI);   // SD is on SDIO; the API keeps an SPI reference

static lv_disp_draw_buf_t draw_buf;
static volatile bool strobe_hold = false;   // beacon strobe: drop LVGL flushes
static volatile bool sleep_hold  = false;   // panel asleep: drop LVGL flushes

static void disp_flush(lv_disp_drv_t* disp, const lv_area_t* area, lv_color_t* color_p) {
    if (strobe_hold || sleep_hold) { lv_disp_flush_ready(disp); return; }
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);

    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.writePixels((uint16_t*)&color_p->full, w * h, true);
    tft.endWrite();

    lv_disp_flush_ready(disp);
}

// -------------------------------------------------------------------------
// FT6336G capacitive touch (I2C 0x38 on the shared bus: SDA 16, SCL 15)
// -------------------------------------------------------------------------
static bool touch_present = false;

static void touch_reset() {
    pinMode(TP_RST, OUTPUT);
    digitalWrite(TP_RST, LOW);
    delay(10);
    digitalWrite(TP_RST, HIGH);
    delay(120);                       // FT6336 needs ~100 ms to boot
}

static bool touch_probe() {
    Wire.beginTransmission(TP_ADDR);
    if (Wire.endTransmission() != 0) return false;
    // Chip ID registers: 0xA3 = chip id (0x64/0x36 on FT6336), 0xA6 = lib version
    Wire.beginTransmission(TP_ADDR);
    Wire.write(0xA3);
    Wire.endTransmission(false);
    if (Wire.requestFrom(TP_ADDR, (uint8_t)1) == 1) {
        uint8_t id = Wire.read();
        Serial.printf("[TOUCH] FT6336G chip id 0x%02X\n", id);
    }
    return true;
}

// Returns true while a finger is down; rx/ry get the raw sensor reading.
static bool touch_get(int16_t* rx, int16_t* ry) {
    Wire.beginTransmission(TP_ADDR);
    Wire.write(0x02);                 // TD_STATUS
    if (Wire.endTransmission(false) != 0) return false;
    if (Wire.requestFrom(TP_ADDR, (uint8_t)5) != 5) return false;

    uint8_t n = Wire.read() & 0x0F;   // number of touch points
    uint8_t xh = Wire.read();
    uint8_t xl = Wire.read();
    uint8_t yh = Wire.read();
    uint8_t yl = Wire.read();
    if (n == 0 || n > 2) return false;

    *rx = (int16_t)(((xh & 0x0F) << 8) | xl);
    *ry = (int16_t)(((yh & 0x0F) << 8) | yl);
    return true;
}

void board_init_io() {
    // Deep-sleep holds survive the wake reset: release them or the backlight
    // stays latched LOW (dark screen) and the LED frozen after every wake.
    gpio_deep_sleep_hold_dis();
    gpio_hold_dis((gpio_num_t)TFT_BL);
    gpio_hold_dis((gpio_num_t)RGB_LED_PIN);

    // WS2812 starts dark; neopixelWrite claims the RMT channel on first use
    pinMode(RGB_LED_PIN, OUTPUT);
    board_set_rgb(false, false, false);

    pinMode(BATT_PIN, INPUT);
    analogSetPinAttenuation(BATT_PIN, ADC_11db);
    pinMode(BOOT_BTN, INPUT_PULLUP);
}

SPIClass& board_sd_spi() { return dummy_spi; }

void board_init_display() {
    tft.init();
    tft.setRotation(DISPLAY_ROTATION);

    // Shared I2C bus: touch controller + audio codec (SDA 16, SCL 15, port 0).
    // Brought up here so audio_start_output() can talk to the ES8311 after.
    Wire.begin(TP_SDA, TP_SCL);
    Wire.setClock(400000);

    pinMode(TP_INT, INPUT);
    touch_reset();
    touch_present = touch_probe();
    Serial.printf("[TOUCH] FT6336G %s\n", touch_present ? "found" : "NOT answering");

    // Two internal-SRAM bands (DMA capable), core-1 renders while core-0 pushes
    lv_color_t* fb1 = (lv_color_t*)heap_caps_malloc(screenWidth * 40 * sizeof(lv_color_t),
                                                   MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    lv_color_t* fb2 = (lv_color_t*)heap_caps_malloc(screenWidth * 40 * sizeof(lv_color_t),
                                                   MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!fb1 || !fb2) Serial.println("[BOARD] framebuffer alloc FAILED");
    lv_disp_draw_buf_init(&draw_buf, fb1, fb2, screenWidth * 40);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = screenWidth;
    disp_drv.ver_res = screenHeight;
    disp_drv.flush_cb = disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);
}

void board_set_backlight(uint8_t pct) {
    tft.setBrightness((uint16_t)pct * 255 / 100);
}

// The strobe fills with the backlight gated off: a 240x320 SPI push still
// takes ~15 ms even at 80 MHz, but it happens in the dark, so the white/black
// transition reads as instant instead of a raster sweep down the panel.
void board_strobe_begin(void) { strobe_hold = true; }
void board_strobe_fill(uint16_t c) {
    tft.setBrightness(0);
    tft.fillScreen(c);
    delay(15);              // one panel refresh (~70 Hz): the new GRAM content
                            // has fully scanned out before the light returns
    tft.setBrightness(255);
}
void board_strobe_end(void) { strobe_hold = false; }

// Display power, beyond the backlight LED. Off sends the ILI9341 to sleep
// (its internal scan stops) and drops LVGL flushes, so the 28.5 min dark
// window before deep sleep costs no SPI traffic; the frame in GRAM is lost,
// so the caller repaints after board_display_power(true).
void board_display_power(bool on) {
    if (!on) {
        sleep_hold = true;
        tft.panel()->setSleep(true);    // ILI9341 SLPIN
    } else {
        tft.panel()->setSleep(false);   // SLPOUT
        delay(130);             // SLPOUT recovery before the next pixel data
        sleep_hold = false;
    }
}

// Deep sleep entry: silence everything that draws power, latch the off levels
// on the pads that float in deep sleep, and arm the touch wake.
// FT6336G INT (GPIO17) is an RTC GPIO driven low while a touch is pending —
// that is the ext0 wake source; waking is a full reset.
void board_enter_deep_sleep(void) {
    Serial.println("[PWR] deep sleep (wake: touch)");
    Serial.flush();

    audio_shutdown();                       // FM8002E off, GPIO1 held high
    board_set_rgb(false, false, false);
    tft.setBrightness(0);
    tft.panel()->setSleep(true);            // ILI9341 SLPIN: stop the panel scan

    // GPIO45 (backlight) and GPIO42 (WS2812) are not RTC pads: they would
    // float during deep sleep, so latch them low first (hold survives the
    // sleep on the S3).
    pinMode(TFT_BL, OUTPUT);   digitalWrite(TFT_BL, LOW);
    pinMode(RGB_LED_PIN, OUTPUT); digitalWrite(RGB_LED_PIN, LOW);
    gpio_hold_en((gpio_num_t)TFT_BL);
    gpio_hold_en((gpio_num_t)RGB_LED_PIN);
    gpio_deep_sleep_hold_en();

    esp_sleep_enable_ext0_wakeup((gpio_num_t)TP_INT, 0);
    esp_deep_sleep_start();
}

int board_ldr_raw() { return -1; }

// TP4054 charger + voltage divider on GPIO9 (ADC1_CH8, works with Wi-Fi on).
// Schematic: BAT+ --R14 200K--> GPIO9 --R15 200K--> GND => divider is exactly
// 1/2, confirmed from the factory schematic (no multimeter needed).
#define BATT_DIV 2.0f
static int batt_pin_mv() {
    uint32_t mv = 0;
    for (int i = 0; i < 16; i++) mv += analogReadMilliVolts(BATT_PIN);
    return (int)(mv / 16);
}
// 1S LiPo voltage -> %, piecewise linear over the real discharge curve
// (a cell is nearly empty at ~3.6 V; a flat linear map over-reports badly).
// Note: while the TP4054 is charging, the terminal voltage sits above the
// resting curve, so the percentage reads higher than the true state of charge.
static const uint16_t batt_curve[][2] = {
    {3300, 0}, {3600, 5}, {3700, 15}, {3750, 25}, {3820, 45},
    {3850, 55}, {3900, 65}, {3980, 80}, {4100, 95}, {4200, 100},
};
int board_batt_pct() {
    int mv = (int)(batt_pin_mv() * BATT_DIV);   // undo the divider
    const int n = sizeof(batt_curve) / sizeof(batt_curve[0]);
    if (mv <= batt_curve[0][0])     return 0;
    if (mv >= batt_curve[n-1][0])   return 100;
    for (int i = 1; i < n; i++)
        if (mv <= batt_curve[i][0])
            return batt_curve[i-1][1] + (mv - batt_curve[i-1][0]) *
                   (batt_curve[i][1] - batt_curve[i-1][1]) /
                   (batt_curve[i][0] - batt_curve[i-1][0]);
    return 100;
}
void board_batt_debug() {
    int pin_mv = batt_pin_mv();
    Serial.printf("[BATT] pin=%d mV, vbat=%d mV (div %.2f), %d%%\n",
                  pin_mv, (int)(pin_mv * BATT_DIV), BATT_DIV, board_batt_pct());
}

// Single WS2812 (GRB wire order handled by the driver). Kept dim: this is a
// status LED on the back of the board, not a flashlight.
void board_set_rgb(bool r, bool g, bool b) {
    neopixelWrite(RGB_LED_PIN, r ? 48 : 0, g ? 48 : 0, b ? 48 : 0);
}

void board_dim_shadow(bool) {}
void board_storage_begin() {}
void board_storage_end() {}

bool board_boot_pressed() { return digitalRead(BOOT_BTN) == LOW; }

bool board_touch_read(int16_t* sx, int16_t* sy, int16_t* raw_x, int16_t* raw_y) {
    if (!touch_present) return false;
    int16_t rx, ry;
    if (!touch_get(&rx, &ry)) return false;

    *raw_x = rx;
    *raw_y = ry;

    int x = rx, y = ry;
    if (TOUCH_SWAP_XY) { int t = x; x = y; y = t; }
    if (TOUCH_MIRROR_X) x = (screenWidth - 1) - x;
    if (TOUCH_MIRROR_Y) y = (screenHeight - 1) - y;
    *sx = constrain(x, 0, (int)screenWidth - 1);
    *sy = constrain(y, 0, (int)screenHeight - 1);
    return true;
}

#endif  // BOARD_ES3C28P
