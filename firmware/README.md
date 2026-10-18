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

You only need a computer, a USB-C cable and
[PlatformIO](https://platformio.org/) — no ESP32 experience required.
Pick **one** of the two paths below.

> **No install at all?** Each GitHub release ships a merged
> `es3c28p-firmware-*.bin`. Flash it straight from your browser with
> [ESP Launchpad](https://espressif.github.io/esp-launchpad/?flashConfigURL=https://raw.githubusercontent.com/AINxStep/star-wars-pocket-datapad/main/docs/esp-launchpad.toml)
> (Chrome/Edge only — WebSerial), or with `esptool` at offset `0x0`:
> `esptool.py --chip esp32s3 write_flash 0x0 <file>.bin`.

### Option A — VS Code + PlatformIO (easiest, graphical)

1. Install [Visual Studio Code](https://code.visualstudio.com/), then open
   it and install the **PlatformIO IDE** extension (Extensions panel,
   search "PlatformIO IDE"). Restart VS Code when it asks.
2. Download this repository: the green **Code → Download ZIP** button on
   GitHub, then unzip it. (Or `git clone` if you already use git.)
3. In VS Code, **File → Open Folder…** and open the `firmware/` folder
   inside the unzipped project (the one that contains `platformio.ini`).
4. Connect the ES3C28P to your computer with the USB-C cable.
5. Click the alien-head **PlatformIO** icon in the left sidebar, then
   under **PROJECT TASKS → esp32-s3-es3c28p → General** click **Upload**.
   The first build downloads the toolchain and libraries (a few minutes);
   later builds take seconds.
6. Done — the datapad reboots into the splash screen when the upload
   finishes.

If the upload fails with "could not open port" or "no serial data
received", the board is not in download mode. Enter it manually: hold the
**BOOT** button (GPIO 0, marked on the board), tap **RST**, then release
BOOT — and click **Upload** again.

### Option B — PlatformIO Core (command line)

1. Install [PlatformIO Core](https://docs.platformio.org/en/latest/core/installation/index.html).
2. Open a terminal in the `firmware/` folder and run:

   ```bash
   pio run -e esp32-s3-es3c28p -t upload --upload-port /dev/cu.usbmodemXXXX
   ```

   Find your port with `pio device list` — the ES3C28P appears as
   `cu.usbmodem*` (macOS/Linux) or `COMx` (Windows). Always pass
   `--upload-port` explicitly — autodetect can pick the wrong board.
3. Same download-mode fallback as above if the upload fails
   (hold **BOOT** → tap **RST** → release **BOOT**).

The project uses the `huge_app` partition table (3 MB for the program) and
stores settings in NVS — reflashing keeps your brightness/volume/language
settings.

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

The datapad works out of the box with built-in synthesized effects — you
do not need a microSD card or any audio files. If you want real clips
(movic lines, actual R2-D2 bleeps, etc.), you can override each sound by
simply copying your own WAV files to a microSD card.

### How it works

Every sound has a fixed file name. At boot the firmware mounts **one**
sound source — the microSD card if one is inserted, otherwise the
internal LittleFS partition. Any file missing on that source (or an
unreadable WAV) falls back to the synthesized clip compiled into the
firmware, so the datapad always has every sound.

### Replacing sounds with your own (microSD — easiest)

1. Format a microSD card as **FAT32** (the default for cards up to 32 GB).
2. In the root of the card, create a folder named exactly `sounds`
   (lowercase).
3. Copy your WAV files into it, renaming them to match the table. Any
   file you do not provide simply keeps its built-in synthesized sound.

```
microSD root/
└── sounds/
    ├── click.wav     UI tap feedback
    ├── r2d2.wav      Comlink screen — R2-D2 button
    ├── blaster.wav   Comlink screen — blaster button
    ├── comlink.wav   Comlink screen — channel button
    ├── alarm.wav     Comlink screen — imperial alarm button
    ├── holo.wav      Hologram projector activation
    ├── coms1.wav     ambient slot 1 (see below)
    ├── coms2.wav     ambient slot 2
    └── coms3.wav     ambient slot 3
```

4. Insert the card and power on (or reset). The serial log prints
   `[AUDIO] microSD mounted` and which files were loaded from where.

**File format:** PCM WAV, 8 or 16 bit, mono or stereo, 4–48 kHz. Keep
effects under ~6 s; longer clips are stored at half sample rate to save
RAM. Use audio you have the rights to — royalty-free clips, your own
recordings or sounds you are licensed to use.

### Ambient chatter (`coms*.wav`) — what it is

While the Home screen is shown, the datapad randomly plays one of the
three `coms` clips after a 1–5 min interval, like background comlink
chatter on a starship console. The three slots take turns in a shuffled
order (never the same clip twice in a row), and a clip is skipped if
another effect is playing.

The repository ships `coms1/2/3.wav` as **one second of silence**, so the
device is completely quiet until you put your own files there — replace
them only if you want the ambient effect.

### Alternative: internal flash (LittleFS)

Only used when **no microSD card is inserted**. If you prefer not to use
a card, put the WAV files in the `firmware/sounds/` folder (same names)
and upload them to the board's LittleFS partition:

```bash
pio run -e esp32-s3-es3c28p -t uploadfs --upload-port /dev/cu.usbmodemXXXX
```

(In VS Code: **PROJECT TASKS → esp32-s3-es3c28p → Platform → Upload
Filesystem Image**.) The microSD route is simpler — it is just a file
copy.

The sample effects in `sounds/` are the same synthesized originals the
firmware uses as fallback; regenerate them with
`python3 tools/gen_sounds.py`.

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
