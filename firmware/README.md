# Datapad firmware (ES3C28P)

[Español](README.es.md) · [Project overview](../README.md)

Touch-screen firmware for the Star Wars pocket datapad, written for the
Arduino framework with **LVGL 8.4** (UI) and **LovyanGFX** (display), built
with **PlatformIO**.

## Features

- **Tactical radar**: holographic 3D wireframe sphere (drag to rotate) with a
  sweeping meridian and fading trail, allied/hostile contacts flying in 3D
  and revealed by the beam, and an info card (nearest hostile, distance,
  bearing).
- **Hologram projector**: ten wireframe 3D models generated in code (Death
  Star, X-Wing, TIE/LN, Rebel crest, Tatooine, Coruscant, Endor, Star
  Destroyer, R2-D2, Hoth) with depth-faded lines, flicker, glitches,
  scanlines and a projector base. Drag to rotate, arrows or a lateral flick
  to change model.
- **Comlink soundboard**: R2-D2, blaster, comlink channel and imperial alarm.
- **P2P link (ESP-NOW)**: optional radio link between two datapads. Toggle it
  from the Comlink screen; a 1 Hz beacon announces the device and the status
  box shows LINK OFF / SEARCHING / PEER OK. With the TX button on, every
  sound-board effect is also sent to the peer, which plays it and shows a
  "message received" banner (alarm in the alert color).
- **Aurebesh keyboard**: glyph keys, selected-letter card (glyph, Latin
  letter, name) and a message line that feeds the signal beacon.
- **Star chart**: stylized spiral galaxy with twelve tappable systems and an
  info card per system; planets with a wireframe model offer a HOLO shortcut
  that jumps straight to the projector (Alderaan opens the Death Star).
- **Signal beacon**: transmits the message composed in the Aurebesh keyboard
  (or "SOS" if empty) as morse code — RGB LED flashes plus an optional
  full-screen strobe.
- **Interface language**: Spanish, English or Aurebesh (English text rendered
  with the Aurebesh font), selectable in Settings and stored in flash.
- **Settings**: brightness, volume, language, color theme (Tatooine or
  Imperial) and power saving.
- **Gestures**: a quick rightward swipe on the top bar goes back to Home; a
  lateral flick on the hologram viewport switches model.
- **Battery monitor**: the board ships with a TP4054 charger and a battery
  divider, so the top bar shows the real charge percentage.
- **Power saving**: with the AHORRO switch on, the screen turns off after
  3 min without touch; after 60 min total the board enters deep sleep and a
  touch on the screen reboots it, like a power switch. The touch that wakes
  the display only wakes it — it never activates what is underneath.
- Splash screen, slide/fade transitions, WS2812 RGB LED feedback.

## Hardware and pin map

Board: **ES3C28P** — ESP32-S3 with a 2.8" ILI9341V 240x320 IPS
display (SPI), FT6336G capacitive touch, ES8311 audio codec, FM8002E
amplifier and speaker, WS2812 RGB LED, microSD slot (SDIO), TP4054 Li-Ion
charger with battery sense and USB-C. The screen runs in portrait so the
USB-C port is at the bottom, matching the enclosure.

| Function | Pins |
| --- | --- |
| Display (SPI, 40 MHz) | SCK 12, MOSI 11, MISO 13, CS 10, DC 46, backlight 45 (active high) |
| Touch FT6336G (I2C) | SDA 16, SCL 15, RST 18, INT 17 |
| Codec ES8311 (I2S) | MCLK 4, BCLK 5, WS 7, DOUT 8; I2C addr 0x18 on the touch bus |
| Amplifier FM8002E | Enable GPIO 1 (active LOW) |
| RGB LED (WS2812) | GPIO 42 |
| Battery sense | GPIO 9 (ADC1_CH8, 1/2 divider) |
| microSD (SDIO 4-bit) | CLK 38, CMD 40, D0 39, D1 41, D2 48, D3 47 |
| BOOT button | GPIO 0 |
| Charging / flashing | USB-C (native USB, TinyUSB) |

## Build and flash

You need [PlatformIO](https://platformio.org/) (CLI or the VS Code
extension). There is a single environment, `esp32-s3-es3c28p`:

```bash
cd firmware
pio run -e esp32-s3-es3c28p -t upload --upload-port /dev/cu.usbmodemXXXX
```

Always pass `--upload-port` explicitly — autodetect can pick the wrong
board. The ES3C28P uses native USB: the port appears as
`/dev/cu.usbmodem*` and TinyUSB auto-reset normally works; if flashing
fails, enter download mode manually (hold **BOOT**, tap **RST**, release
BOOT).

The project uses the `huge_app` partition table (3 MB for the program) and
stores settings in NVS.

### Serial diagnostics

TinyUSB drops boot-time prints while no host holds the port open. Once you
attach a serial monitor, `loop()` re-prints the audio load report
(`[AUDIO] source … clips`) once and the battery status (`[BATT]`) every
10 s:

```bash
pio device monitor -b 115200 -p /dev/cu.usbmodemXXXX
```

In deep sleep the USB console disappears; attaching a monitor resets the
board, so flash only while it is awake (or in manual download mode).

## Custom sounds (optional)

At boot the firmware looks for WAV clips in this order and falls back to a
built-in synthesized version for any that is missing:

1. `/sounds/<name>.wav` on the microSD card (SDIO, 4-bit with 1-bit retry)
2. `<name>.wav` in the internal LittleFS partition
3. The synthesized clip compiled into the firmware

```
click.wav  r2d2.wav  blaster.wav  comlink.wav  alarm.wav  holo.wav
coms1.wav  coms2.wav  coms3.wav
```

`coms*.wav` are the ambient comlink chatter slots: while the Home screen is
shown, one of them plays after a random 1-5 min interval (the three take
turns in a shuffled order, never the same one twice in a row; skipped while
another effect is busy). The repository ships them as one second of
silence, so there is no ambient sound until you put your own clips there.
Use audio you have the rights to, and keep it out of commits (for example
`git update-index --skip-worktree firmware/sounds/coms1.wav` and the other
two).

To upload the `sounds/` folder to the LittleFS partition (once, and
whenever they change — a microSD card is easier: just copy the files):

```bash
pio run -e esp32-s3-es3c28p -t uploadfs --upload-port /dev/cu.usbmodemXXXX
```

Formats: PCM WAV, 8 or 16 bit, mono or stereo, 4-48 kHz, up to about 6 s
for the effects. Clips longer than 20 000 samples are stored at half
sample rate to save RAM. Sample sounds (synthesized, original; the ambient
slots are silent) are in `sounds/`; regenerate them with
`python3 tools/gen_sounds.py`. The serial log shows which files were loaded
and from where.

## Color themes

Two themes, selectable in **Settings** (TATOOINE / IMPERIAL) and stored in
flash; Tatooine is the default. Changing it rebuilds every screen, like the
language.

| Theme | Look |
| :--- | :--- |
| **Tatooine** | Dark desert brown, twin-sun amber accent, turquoise secondary text, red alerts, classic blue holograms |
| **Imperial** | Near-black steel, imperial red accent, ice-grey secondary text, amber alerts (so they stand out from the red accent), ice-blue holograms |

Colors are defined by role in `struct UiTheme` (`src/ui/ui_common.h`) and
read through the `COLOR_*` macros: `ACCENT` (borders, titles, selected
controls), `DATA` (secondary labels), `ALERT` (hostile contacts, red
alert), `FRIEND` (allied contacts), `TEXT`/`BG`/`PANEL`, and
`HOLO`/`CHAMBER`/`HILITE` for the hologram and radar projections. Use these
roles instead of literal colors in new UI code. To add a theme: add a
`THEME_*` entry to the enum, a row to the `themes[]` table, bump the
`constrain()` in `settings.h` and add a button in `add_theme_selector()`
(`screens/settings.cpp`).

## Code structure

```
src/main.cpp             Setup, LVGL glue, touch input, display sleep, deep sleep, main loop
src/audio.cpp            PCM sound engine (ES8311 codec + SD/LittleFS WAV loading)
src/link.cpp             ESP-NOW beacon link between datapads
src/board_es3c28p.cpp    Board layer: panel, touch, backlight, codec power, deep sleep
src/i18n.cpp             Translated strings (ES / EN / Aurebesh)
src/ui/ui.cpp            UI lifecycle, navigation, language rebuild
src/ui/ui_common.*       Theme, styles, top bar, fonts, screen registry
src/ui/screens/*.cpp     One file per screen: splash, home, radar, holo (+ models),
                         comlink, aurebesh, settings, calibration, starchart, beacon
include/                 board.h abstraction, hardware helpers, settings, LVGL
                         config, generated data
tools/                   Generators (logo, hologram crest, sounds, Aurebesh fonts)
sounds/                  Generated sample sound effects
```

**Add a screen:** create `src/ui/screens/<name>.cpp` exposing
`lv_obj_t* screen_<name>_create(void)`, add it to `enum UiScreen`
(`ui_common.h`) and to `create_screens()` in `ui.cpp`, then call
`ui_goto(SCR_<NAME>)`. Timers created by a screen must be created only once
(screens are rebuilt when the language changes).

**Add or change text:** add a `StrId` in `include/i18n.h` and a row in the
table in `src/i18n.cpp` (Spanish, English and an optional shorter Aurebesh
wording, since Aurebesh glyphs are about 1.7x wider than Latin letters).
Use `tr(S_...)` for text and `ui_font(size)` for fonts.

## Generated assets and tools

| Tool | Produces |
| --- | --- |
| `tools/gen_logo.py` | `include/rebel_logo.h`: splash logo bitmap, rasterized from the SVG |
| `tools/gen_holo_crest.py` | `include/holo_crest.h`: simplified crest outline for the hologram |
| `tools/gen_sounds.py` | `sounds/*.wav`: synthesized sound effects |
| `tools/gen_aurebesh_font.sh` | `src/ui/font_aurebesh_*.c` and `include/aurebesh_font.h` (needs Node.js and an Aurebesh font file) |

The Aurebesh fonts are committed, so you only need the script to change
sizes or the source font. Without the fonts the UI still builds and shows
Latin letters. The glyphs come from **Aurebesh AF** (see
[NOTICE](../NOTICE.md)).

## Technical notes

- **Audio through the ES8311 codec.** A task pinned to core 1 (priority 5,
  above `loop()` so system tasks cannot starve it) streams 16 kHz PCM over
  I2S: MCLK 4, BCLK 5, WS 7, DOUT 8. The codec lives at I2C 0x18 on the
  touch bus (SCL 15 / SDA 16, owned by `Wire`), and the FM8002E amplifier
  is enabled with GPIO 1 (active LOW). Underruns repeat the last buffer
  instead of writing zeros.
- **Sound clips in PSRAM.** Decoded WAVs go to `MALLOC_CAP_SPIRAM` so the
  display's render buffers keep internal RAM; ambient chatter can use the
  full 6 s clip.
- **Deep sleep.** With power saving on, 3 min idle sleeps the display
  (`board_display_power(false)` cuts panel and backlight); at 60 min
  `audio_shutdown()` mutes the codec and latches the amp pin with a GPIO
  hold before `board_enter_deep_sleep()` parks the SoC with the FT6336G
  INT line (GPIO 17, RTC-capable) as the wake source. **GPIO holds set
  before deep sleep survive the wake reset**, so the boot path releases
  them (`gpio_deep_sleep_hold_dis()` + `gpio_hold_dis()`) or the backlight
  and amp stay latched off. Bench-test the pipeline without editing code:
  `PLATFORMIO_BUILD_FLAGS="-D SLEEP_TIMEOUT_MS=15000 -D DEEP_SLEEP_TIMEOUT_MS=45000"`.
- **Flash writes disable the CPU cache** (NVS, LittleFS): the board layer
  stops display DMA around them (`board_storage_begin/end()`); sound clips
  are read at boot before the panel starts streaming.
- **Memory.** LVGL has a 64 KB pool (about 40 KB are used once every screen
  is built). Changing the language rebuilds all screens: the old ones are
  deleted first because two full sets do not fit.
- **Hologram renderer.** Models are procedural vertex/edge lists, rotated
  and projected on the CPU every frame and drawn with LVGL line primitives
  in a custom draw callback.
- **ESP-NOW link.** Enabling it brings the Wi-Fi driver up in station mode
  (no AP needed) and costs roughly 40-60 KB of heap. The radio and its
  callbacks run on core 0 while the audio task stays on core 1. With a
  single device you can verify the link starts and beacons are sent (serial
  log); confirming peer detection needs a second datapad (or two simulator
  tabs).

## Troubleshooting

- **No sound**: check the volume in Settings; the serial log prints the
  audio source (`[AUDIO] source: …`) once a monitor is attached. With clips
  on microSD, make sure the files are in `/sounds/` and are PCM WAV.
- **Upload fails**: pass `--upload-port` explicitly, or enter download mode
  (hold BOOT → tap RST → release BOOT).
- **Display or amp dead after waking from deep sleep**: the GPIO holds were
  not released on boot — see the deep-sleep note above.
- **Serial shows nothing**: open the port after boot (TinyUSB drops early
  prints); the firmware re-prints the audio report and starts `[BATT]`
  logs once a host attaches.
