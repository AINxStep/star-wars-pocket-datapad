#include "starwars_audio.h"
#include <LittleFS.h>
#include <SD_MMC.h>
#include <Wire.h>
#include <driver/i2s_std.h>
#include <driver/gpio.h>
#include <esp_heap_caps.h>
#include <math.h>

#define AUDIO_RATE      16000
#define AUDIO_CHUNK     256                 // frames per DMA write
#define MAX_SD_SAMPLES  96000               // ~6 s at 16 kHz per effect
#define AMBIENT_MAX_SAMPLES MAX_SD_SAMPLES    // PSRAM: ambient chatter plays in full (6-12 s)
#define LONG_CLIP_SAMPLES 20000             // clips longer than this are halved in RAM
#define SOUNDS_DIR      "/sounds/"

// Where decoded clips live. On the S3 they go to PSRAM: internal RAM is needed
// by the display's render buffers, which are allocated after the clips load.
#define CLIP_HEAP_CAPS (MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)

struct Sound {
    uint8_t* data = nullptr;   // unsigned 8-bit mono, 128 = silence
    uint32_t len = 0;
    uint16_t rate = AUDIO_RATE;
    bool from_sd = false;
};

static Sound sounds[SND_COUNT];
static const char* sound_names[SND_COUNT] = { "click", "r2d2", "blaster", "comlink", "alarm", "holo",
                                              "coms1", "coms2", "coms3" };

static volatile uint8_t volume_pct = 60;
static volatile bool playing = false;
static QueueHandle_t play_queue;

// Where the clips actually came from — reported by audio_print_load_report()
static const char* audio_src = "none";
static int audio_loaded = -1;


// -------------------------------------------------------------------------
// Built-in synthesized fallback
// -------------------------------------------------------------------------
struct Step { uint16_t freq; uint16_t ms; };   // freq 0 = silence

// Renders a tone sequence as a lightly overdriven sine with phase continuity
// and a short ramp on every step so there are no clicks between notes.
static void render_steps(Sound& snd, const Step* steps, int n) {
    uint32_t total = 0;
    for (int i = 0; i < n; i++) total += (uint32_t)steps[i].ms * AUDIO_RATE / 1000;

    uint8_t* buf = (uint8_t*)malloc(total);
    if (!buf) return;

    float phase = 0;
    uint32_t pos = 0;
    for (int i = 0; i < n; i++) {
        uint32_t len = (uint32_t)steps[i].ms * AUDIO_RATE / 1000;
        uint32_t ramp = min<uint32_t>(32, len / 4);
        float dphi = 2.0f * (float)M_PI * steps[i].freq / AUDIO_RATE;
        for (uint32_t k = 0; k < len; k++, pos++) {
            if (steps[i].freq == 0) { buf[pos] = 128; continue; }
            float env = 1.0f;
            if (k < ramp) env = (float)k / ramp;
            else if (len - k < ramp) env = (float)(len - k) / ramp;
            phase += dphi;
            float x = 1.5f * sinf(phase) * env;      // overdrive adds loudness/grit
            if (x > 1) x = 1; else if (x < -1) x = -1;
            buf[pos] = (uint8_t)(128 + 120 * x);
        }
    }
    snd.data = buf;
    snd.len = total;
    snd.rate = AUDIO_RATE;
    snd.from_sd = false;
}

static void render_builtin(SoundId id) {
    Step steps[128];
    int n = 0;
    switch (id) {
        case SND_CLICK:
            steps[n++] = { 2200, 12 };
            break;
        case SND_COMLINK:
            steps[n++] = { 1850, 40 };
            steps[n++] = { 2400, 50 };
            steps[n++] = { 1200, 30 };
            break;
        case SND_BLASTER:
            for (int f = 3000; f > 300; f -= 120) steps[n++] = { (uint16_t)f, 4 };
            break;
        case SND_R2D2: {
            static const uint16_t tones[] = { 1800, 2600, 1400, 2200, 3100, 1950, 2800 };
            for (int i = 0; i < 7; i++) {
                steps[n++] = { tones[i], (uint16_t)(45 + i * 10) };
                steps[n++] = { 0, 15 };
            }
            break;
        }
        case SND_ALARM:
            for (int rep = 0; rep < 2; rep++) {
                for (int f = 600; f < 1500; f += 40) steps[n++] = { (uint16_t)f, 8 };
                for (int f = 1500; f > 600; f -= 40) steps[n++] = { (uint16_t)f, 8 };
            }
            break;
        case SND_HOLO:   // rising shimmer
            for (int f = 300; f < 2200; f += 90) steps[n++] = { (uint16_t)f, 12 };
            break;
        case SND_COMS1:  // radio squawk fallbacks (used only without the SD files)
            steps[n++] = { 1600, 25 }; steps[n++] = { 0, 12 };
            steps[n++] = { 2100, 30 }; steps[n++] = { 1400, 20 };
            break;
        case SND_COMS2:
            steps[n++] = { 900, 35 };  steps[n++] = { 0, 15 };
            steps[n++] = { 1900, 20 }; steps[n++] = { 0, 10 };
            steps[n++] = { 1200, 30 };
            break;
        case SND_COMS3:
            steps[n++] = { 2400, 15 }; steps[n++] = { 0, 10 };
            steps[n++] = { 1700, 25 }; steps[n++] = { 0, 10 };
            steps[n++] = { 2600, 15 }; steps[n++] = { 0, 10 };
            steps[n++] = { 1300, 25 };
            break;
        default:
            return;
    }
    render_steps(sounds[id], steps, n);
}

// -------------------------------------------------------------------------
// WAV loading (SD card)
// -------------------------------------------------------------------------
static uint32_t rd32(const uint8_t* p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24); }
static uint16_t rd16(const uint8_t* p) { return p[0] | (p[1] << 8); }

// Parses a PCM WAV and converts it to unsigned 8-bit mono, keeping at most
// `cap` frames. Returns false on unsupported/invalid files (the built-in
// sound is kept in that case).
static bool load_wav(File& f, Sound& out, uint32_t cap) {
    uint8_t hdr[12];
    if (f.read(hdr, 12) != 12 || memcmp(hdr, "RIFF", 4) || memcmp(hdr + 8, "WAVE", 4)) {
        Serial.printf("[AUDIO]   rejected: not RIFF/WAVE (%.4s/%.4s)\n", hdr, hdr + 8);
        return false;
    }

    uint16_t channels = 0, bits = 0, rate_hz = 0;
    uint32_t rate = 0;
    bool have_fmt = false;

    for (;;) {
        uint8_t ch[8];
        if (f.read(ch, 8) != 8) {
            Serial.println("[AUDIO]   rejected: no data chunk");
            return false;
        }
        uint32_t size = rd32(ch + 4);

        if (!memcmp(ch, "fmt ", 4)) {
            uint8_t fmt[16];
            if (size < 16 || f.read(fmt, 16) != 16) {
                Serial.printf("[AUDIO]   rejected: fmt chunk size %u\n", size);
                return false;
            }
            if (rd16(fmt) != 1) {                             // PCM only
                Serial.printf("[AUDIO]   rejected: fmt tag %u (not PCM)\n", rd16(fmt));
                return false;
            }
            channels = rd16(fmt + 2);
            rate = rd32(fmt + 4);
            bits = rd16(fmt + 14);
            if (size > 16) f.seek(f.position() + size - 16);
            have_fmt = true;
        } else if (!memcmp(ch, "data", 4)) {
            if (!have_fmt || channels < 1 || channels > 2 || (bits != 8 && bits != 16)) {
                Serial.printf("[AUDIO]   rejected: ch %u bits %u\n", channels, bits);
                return false;
            }
            if (rate < 4000 || rate > 48000) {
                Serial.printf("[AUDIO]   rejected: rate %u\n", rate);
                return false;
            }
            rate_hz = rate;

            uint32_t frame_bytes = channels * bits / 8;
            uint32_t frames_total = size / frame_bytes;
            uint32_t wanted = frames_total < cap ? frames_total : cap;
            uint32_t frames = wanted;
            if (frames == 0) {
                Serial.printf("[AUDIO]   rejected: data chunk %u bytes\n", size);
                return false;
            }

            // If the full clip does not fit in the fragmented heap, halve the
            // frame count and retry: a shorter clip beats the built-in fallback
            uint8_t* buf = (uint8_t*)heap_caps_malloc(frames, CLIP_HEAP_CAPS);
            while (!buf && frames >= 8192) {   // don't shrink tiny clips to nothing
                frames /= 2;
                buf = (uint8_t*)heap_caps_malloc(frames, CLIP_HEAP_CAPS);
            }
            if (!buf) {
                Serial.printf("[AUDIO]   rejected: no heap left\n");
                return false;
            }
            if (frames < wanted) Serial.printf("[AUDIO]   note: clip truncated to %u samples by heap\n", frames);

            uint8_t chunk[512];
            uint32_t done = 0;
            while (done < frames) {
                uint32_t want = min<uint32_t>((frames - done) * frame_bytes, sizeof(chunk));
                want -= want % frame_bytes;
                int got = f.read(chunk, want);
                if (got < (int)frame_bytes) break;
                for (int i = 0; i + (int)frame_bytes <= got && done < frames; i += frame_bytes) {
                    int v;
                    if (bits == 8) {
                        v = chunk[i];
                        if (channels == 2) v = (v + chunk[i + 1]) / 2;
                    } else {
                        int l = (int16_t)rd16(chunk + i);
                        if (channels == 2) l = (l + (int16_t)rd16(chunk + i + 2)) / 2;
                        v = (l >> 8) + 128;
                    }
                    buf[done++] = (uint8_t)v;
                }
            }
            if (done == 0) {
                Serial.printf("[AUDIO]   rejected: data chunk read failed\n");
                free(buf);
                return false;
            }

            // RAM is tight (TLS needs ~95 KB free): long clips are stored at half rate
            if (done > LONG_CLIP_SAMPLES && rate_hz >= 12000) {
                for (uint32_t i = 0; i < done / 2; i++) buf[i] = (buf[2 * i] + buf[2 * i + 1]) / 2;
                done /= 2;
                rate_hz /= 2;
            }

            free(out.data);
            out.data = buf;
            out.len = done;
            out.rate = rate_hz;
            out.from_sd = true;
            return true;
        } else {
            f.seek(f.position() + size + (size & 1));
        }
    }
}

// -------------------------------------------------------------------------
// ES3C28P: real microSD slot wired to the SDIO host (4-bit: CLK 38, CMD 40,
// D0 39, D1 41, D2 48, D3 47). Clips load from /sounds/<name>.wav on the card;
// with no card inserted they fall back to the LittleFS "spiffs" partition
// (uploaded with `pio run -e esp32-s3-es3c28p -t uploadfs`), then to the
// built-in synth sounds.
// -------------------------------------------------------------------------
static int load_sd_sounds(SPIClass&) {
    FS* fs = nullptr;
    SD_MMC.setPins(38, 40, 39, 41, 48, 47);   // clk, cmd, d0..d3
    if (SD_MMC.begin()) {
        fs = &SD_MMC;
        audio_src = "microSD 4-bit";
        Serial.println("[AUDIO] microSD mounted (SDIO 4-bit)");
    } else if (SD_MMC.begin("/sdcard", true)) {   // retry in 1-bit mode: only CLK/CMD/D0
        fs = &SD_MMC;
        audio_src = "microSD 1-bit";
        Serial.println("[AUDIO] microSD mounted (SDIO 1-bit)");
    } else if (LittleFS.begin(false, "/littlefs", 10, "spiffs")) {
        fs = &LittleFS;
        audio_src = "LittleFS";
        Serial.println("[AUDIO] No SD card, clips from flash LittleFS");
    } else {
        Serial.println("[AUDIO] No SD card and no sound filesystem, using built-in sounds");
        audio_src = "built-in synth";
        audio_loaded = 0;
        return 0;
    }

    Serial.printf("[AUDIO] heap %u free, %u largest\n",
                  (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxAllocHeap());

    static const uint8_t load_order[SND_COUNT] = {
        SND_ALARM, SND_R2D2, SND_COMS1, SND_COMS2, SND_COMS3,
        SND_HOLO, SND_BLASTER, SND_COMLINK, SND_CLICK
    };
    const bool from_card = (fs == &SD_MMC);
    int loaded = 0;
    for (int k = 0; k < SND_COUNT; k++) {
        int i = load_order[k];
        String path = from_card ? String(SOUNDS_DIR) + sound_names[i] + ".wav"
                                : String("/") + sound_names[i] + ".wav";
        File f = fs->open(path);
        if (!f) continue;
        uint32_t cap = (i >= SND_COMS1) ? AMBIENT_MAX_SAMPLES : MAX_SD_SAMPLES;
        if (load_wav(f, sounds[i], cap)) {
            Serial.printf("[AUDIO] %s: %u samples @ %u Hz from %s\n",
                          path.c_str(), sounds[i].len, sounds[i].rate,
                          from_card ? "SD" : "flash");
            loaded++;
        } else {
            Serial.printf("[AUDIO] %s: unsupported WAV, using built-in\n", path.c_str());
        }
        f.close();
    }
    if (from_card) SD_MMC.end(); else LittleFS.end();
    audio_loaded = loaded;
    return loaded;
}


// -------------------------------------------------------------------------
// ES8311 codec on I2S (16 kHz, MCLK 256*fs). It shares the touch I2C bus
// (SCL 15 / SDA 16) brought up by board_init_display(); the FM8002E amp
// enable is GPIO1, LOW = on. The bus is always up before this runs
// (audio_start_output).
// -------------------------------------------------------------------------
#define S3_CODEC_ADDR 0x18          // ES8311, CE pin low
#define S3_OUT_RATE   16000

#define S3_I2S_MCLK   GPIO_NUM_4
#define S3_I2S_BCLK   GPIO_NUM_5
#define S3_I2S_WS     GPIO_NUM_7
#define S3_I2S_DOUT   GPIO_NUM_8  // playback data to the ES8311 (the IO table's
                                // direction labels for GPIO6/8 are backwards;
                                // the factory xiaozhi config is authoritative)
#define S3_PA_PIN     1           // FM8002E enable, active LOW

static i2s_chan_handle_t tx_chan = nullptr;

// The ES3C28P shares I2C port 0 between the FT6336G touch and the codec; the
// bus is owned by Wire (started in board_init_display), so codec registers
// go through the Wire API here.
static esp_err_t i2c_wr(uint8_t addr, uint8_t reg, uint8_t val) {
    Wire.beginTransmission(addr);
    Wire.write(reg);
    Wire.write(val);
    return Wire.endTransmission() == 0 ? ESP_OK : ESP_FAIL;
}
static esp_err_t i2c_rd(uint8_t addr, uint8_t reg, uint8_t* val) {
    Wire.beginTransmission(addr);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) return ESP_FAIL;
    if (Wire.requestFrom(addr, (uint8_t)1) != 1) return ESP_FAIL;
    *val = Wire.read();
    return ESP_OK;
}

// Register sequence from Espressif's es8311 driver (esp-bsp), MCLK pin,
// slave I2S 16-bit, DAC only; coefficients for MCLK 4.096 MHz / 16 kHz.
static bool codec_init() {
    uint8_t v;
    if (i2c_rd(S3_CODEC_ADDR, 0xFD, &v) != ESP_OK) {          // chip id register: any ACK means present
        Serial.println("[AUDIO] ES8311 not answering on I2C 0x18");
        return false;
    }
    bool ok = true;
    ok &= i2c_wr(S3_CODEC_ADDR, 0x00, 0x1F) == ESP_OK;         // reset
    vTaskDelay(pdMS_TO_TICKS(20));
    ok &= i2c_wr(S3_CODEC_ADDR, 0x00, 0x00) == ESP_OK;
    ok &= i2c_wr(S3_CODEC_ADDR, 0x00, 0x80) == ESP_OK;         // power on
    ok &= i2c_wr(S3_CODEC_ADDR, 0x01, 0x3F) == ESP_OK;         // all clocks on, MCLK from the pin
    ok &= i2c_rd(S3_CODEC_ADDR, 0x06, &v) == ESP_OK;
    ok &= i2c_wr(S3_CODEC_ADDR, 0x06, v & ~0x20) == ESP_OK;    // SCLK not inverted
    // 4.096 MHz / 16 kHz: pre_div 1, pre_multi 1x, adc/dac div 1, single speed, bclk div 4, OSR 0x10
    ok &= i2c_rd(S3_CODEC_ADDR, 0x02, &v) == ESP_OK;
    ok &= i2c_wr(S3_CODEC_ADDR, 0x02, (v & 0x07) | ((1 - 1) << 5) | (0 << 3)) == ESP_OK;
    ok &= i2c_wr(S3_CODEC_ADDR, 0x03, 0x10) == ESP_OK;
    ok &= i2c_wr(S3_CODEC_ADDR, 0x04, 0x10) == ESP_OK;
    ok &= i2c_wr(S3_CODEC_ADDR, 0x05, 0x00) == ESP_OK;
    ok &= i2c_rd(S3_CODEC_ADDR, 0x06, &v) == ESP_OK;
    ok &= i2c_wr(S3_CODEC_ADDR, 0x06, (v & 0xE0) | (4 - 1)) == ESP_OK;
    ok &= i2c_rd(S3_CODEC_ADDR, 0x07, &v) == ESP_OK;
    ok &= i2c_wr(S3_CODEC_ADDR, 0x07, (v & 0xC0) | 0x00) == ESP_OK;
    ok &= i2c_wr(S3_CODEC_ADDR, 0x08, 0xFF) == ESP_OK;
    ok &= i2c_rd(S3_CODEC_ADDR, 0x00, &v) == ESP_OK;
    ok &= i2c_wr(S3_CODEC_ADDR, 0x00, v & 0xBF) == ESP_OK;     // serial port in slave mode
    ok &= i2c_wr(S3_CODEC_ADDR, 0x09, 3 << 2) == ESP_OK;       // SDP in: 16-bit I2S
    ok &= i2c_wr(S3_CODEC_ADDR, 0x0A, 3 << 2) == ESP_OK;       // SDP out: 16-bit I2S
    ok &= i2c_wr(S3_CODEC_ADDR, 0x0D, 0x01) == ESP_OK;         // power up analog
    ok &= i2c_wr(S3_CODEC_ADDR, 0x0E, 0x02) == ESP_OK;
    ok &= i2c_wr(S3_CODEC_ADDR, 0x12, 0x00) == ESP_OK;         // power up DAC
    ok &= i2c_wr(S3_CODEC_ADDR, 0x13, 0x10) == ESP_OK;         // output to the headphone/speaker driver
    ok &= i2c_wr(S3_CODEC_ADDR, 0x1C, 0x6A) == ESP_OK;
    ok &= i2c_wr(S3_CODEC_ADDR, 0x37, 0x08) == ESP_OK;         // bypass the DAC equalizer
    ok &= i2c_wr(S3_CODEC_ADDR, 0x32, 0xB2) == ESP_OK;         // DAC volume ~70 % (user volume is applied in software)
    ok &= i2c_rd(S3_CODEC_ADDR, 0x31, &v) == ESP_OK;
    ok &= i2c_wr(S3_CODEC_ADDR, 0x31, v & ~0x60) == ESP_OK;    // unmute
    return ok;
}

// Power amplifier
static bool amp_enable() {
    // FM8002E shutdown pin on GPIO1, LOW = amplifier on (IO table).
    // gpio_hold_dis releases the latch left by deep sleep on wake.
    gpio_hold_dis((gpio_num_t)S3_PA_PIN);
    pinMode(S3_PA_PIN, OUTPUT);
    digitalWrite(S3_PA_PIN, LOW);
    return true;
}

static void audio_task(void*) {
    static int16_t frames[AUDIO_CHUNK * 2];
    const Sound* cur = nullptr;
    uint32_t pos = 0, frac = 0, step = 0;           // read position (clip samples) and 16.16 phase step

    for (;;) {
        uint8_t id;
        // Idle: sleep until a request arrives (the I2S channel streams zeros by itself)
        if (xQueueReceive(play_queue, &id, cur ? 0 : portMAX_DELAY) == pdTRUE && id < SND_COUNT && sounds[id].data) {
            cur = &sounds[id];
            pos = frac = 0;
            step = (uint32_t)(cur->rate * 65536.0f / S3_OUT_RATE);
            playing = true;
        }
        if (!cur) continue;

        uint32_t vol = volume_pct;
        for (int i = 0; i < AUDIO_CHUNK; i++) {
            int v = 0;
            if (cur) {
                if (pos < cur->len) {
                    int a = cur->data[pos];
                    int b = (pos + 1 < cur->len) ? cur->data[pos + 1] : 128;
                    int smp = a + (((b - a) * (int)frac) >> 16);    // linear interpolation
                    v = ((smp - 128) * (int)(vol * vol)) / 10000;   // -128..127 scaled by volume^2
                    frac += step;
                    pos += frac >> 16;
                    frac &= 0xFFFF;
                } else {
                    cur = nullptr;
                    playing = false;
                }
            }
            frames[2 * i] = frames[2 * i + 1] = (int16_t)(v << 8);
        }
        size_t written;
        i2s_channel_write(tx_chan, frames, sizeof(frames), &written, portMAX_DELAY);
    }
}

static void start_engine() {
    // The queue exists from audio_init() so requests before the output is up are harmless
    play_queue = xQueueCreate(1, sizeof(uint8_t));
}

// Called once the display library has brought up the shared I2C bus
void audio_start_output() {
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chan_cfg.auto_clear = true;                     // zeros while idle: silence, no clicks
    chan_cfg.dma_desc_num = 6;
    chan_cfg.dma_frame_num = AUDIO_CHUNK;
    if (i2s_new_channel(&chan_cfg, &tx_chan, NULL) != ESP_OK) {
        Serial.println("[AUDIO] i2s_new_channel failed");
        return;
    }
    i2s_std_config_t std_cfg = {};
    std_cfg.clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(S3_OUT_RATE);          // MCLK = 256 * fs
    std_cfg.slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO);
    std_cfg.gpio_cfg.mclk = S3_I2S_MCLK;
    std_cfg.gpio_cfg.bclk = S3_I2S_BCLK;
    std_cfg.gpio_cfg.ws = S3_I2S_WS;
    std_cfg.gpio_cfg.dout = S3_I2S_DOUT;
    std_cfg.gpio_cfg.din = I2S_GPIO_UNUSED;
    if (i2s_channel_init_std_mode(tx_chan, &std_cfg) != ESP_OK || i2s_channel_enable(tx_chan) != ESP_OK) {
        Serial.println("[AUDIO] I2S init failed");
        return;
    }
    vTaskDelay(pdMS_TO_TICKS(10));                  // let MCLK settle before talking to the codec

    bool codec = codec_init();
    bool amp = amp_enable();
    Serial.printf("[AUDIO] S3 output: codec %s, amplifier %s\n", codec ? "ok" : "FAILED", amp ? "on" : "NOT enabled");
    xTaskCreatePinnedToCore(audio_task, "audio", 4096, NULL, 5, NULL, 1);
}

// -------------------------------------------------------------------------
// Public API
// -------------------------------------------------------------------------
void audio_init(SPIClass& sd_spi) {
    for (int i = 0; i < SND_COUNT; i++) render_builtin((SoundId)i);
    load_sd_sounds(sd_spi);
    start_engine();
}


// Called before entering deep sleep: powers the codec down and releases the
// FM8002E shutdown pin (active-LOW, so HIGH mutes it). GPIO1 is an RTC pad on
// the S3 and would keep its level anyway; the hold makes that explicit.
void audio_shutdown() {
    i2c_wr(S3_CODEC_ADDR, 0x00, 0x1F);            // ES8311 reset/power-down
    pinMode(S3_PA_PIN, OUTPUT);
    digitalWrite(S3_PA_PIN, HIGH);
    gpio_hold_en((gpio_num_t)S3_PA_PIN);
}

void audio_set_volume(uint8_t pct) { volume_pct = pct > 100 ? 100 : pct; }

// On USB-native boards the console only exists while a host has it open, so
// boot-time prints are silently dropped. main.cpp re-emits this report the
// first time Serial comes up.
void audio_print_load_report() {
    if (audio_loaded < 0) return;
    Serial.printf("[AUDIO] source: %s — %d/%d clips loaded\n", audio_src, audio_loaded, (int)SND_COUNT);
}

bool audio_busy() { return playing; }

uint32_t audio_play(SoundId id) {
    if (id >= SND_COUNT || !sounds[id].data || !play_queue) return 0;
    uint8_t v = (uint8_t)id;
    xQueueOverwrite(play_queue, &v);   // latest request wins
    return (uint32_t)sounds[id].len * 1000UL / sounds[id].rate;
}
