#pragma once
#include <Arduino.h>
#include <SPI.h>

// =========================================================================
// Sound engine
// All effects are PCM clips (8-bit mono) played through the ESP32's internal
// DAC (GPIO 26 -> speaker amp) by a background task, so playback never
// blocks the UI.
//
// At boot, each effect is loaded from /sounds/<name>.wav on the microSD
// (8/16-bit PCM, mono or stereo, any rate up to 48 kHz). Missing files fall
// back to a built-in synthesized version, so the datapad works without a card.
// =========================================================================

enum SoundId {
    SND_CLICK,
    SND_R2D2,
    SND_BLASTER,
    SND_COMLINK,
    SND_ALARM,
    SND_HOLO,
    SND_COMS1,      // ambient comlink chatter, played at random on Home
    SND_COMS2,
    SND_COMS3,
    SND_COUNT
};

// Renders the built-in effects, loads WAVs from the SD card (using `sd_spi`
// temporarily; the bus is released before returning) and starts playback.
void audio_init(SPIClass& sd_spi);

// Starts the sound output: brings up the ES8311 codec, which needs the I2C
// bus created by the display, so call it after board_init_display().
void audio_start_output();

void audio_set_volume(uint8_t pct);   // 0..100
bool audio_busy();

// Re-prints the boot-time clip source/count. Called from loop() once a host
// opens the USB console (the early boot prints are dropped with TinyUSB).
void audio_print_load_report();

// Powers down the codec and amplifier for deep sleep.
void audio_shutdown();

// Starts an effect (interrupting the current one); returns its duration in ms
uint32_t audio_play(SoundId id);

inline uint32_t sfx_click()          { return audio_play(SND_CLICK); }
inline uint32_t sfx_r2d2()           { return audio_play(SND_R2D2); }
inline uint32_t sfx_blaster()        { return audio_play(SND_BLASTER); }
inline uint32_t sfx_comlink_chirp()  { return audio_play(SND_COMLINK); }
inline uint32_t sfx_red_alert()      { return audio_play(SND_ALARM); }
inline uint32_t sfx_holo()           { return audio_play(SND_HOLO); }
inline uint32_t sfx_coms(int i)      { return audio_play((SoundId)(SND_COMS1 + (i % 3))); }
