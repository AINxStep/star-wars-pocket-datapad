# Web simulator

Runs the datapad's real interface in a browser, locally. The firmware's own
UI code (screens, texts, themes, Aurebesh fonts, the vector rasterizer) is
compiled to WebAssembly with [Emscripten](https://emscripten.org/) together
with LVGL 8.4; only the hardware layer is replaced by a browser one.

- The layout is the one on the hardware: **ES3C28P**, 240x320 portrait.
- The mouse (or a finger) is the touch screen; drag to rotate holograms and
  the radar, swipe right on the top bar to go back.
- Sound through the Web Audio API, with the same clips as the firmware.
- The status LED and the screen brightness (and screen-off sleep) are drawn
  on the page. Serial output of the firmware appears in the side panel.
- Settings (language, theme, volume, ...) persist in the browser's
  `localStorage`.
- **P2P link between two datapads:** press "Segundo datapad" to open another
  tab, turn on P2P in Comlink on both and send sounds between them (a
  `BroadcastChannel` stands in for ESP-NOW, same-browser only).

## Build and run

Needs `git`, `curl`, `make` and a Python 3.10+ (PlatformIO's own Python is
used if present). The first build downloads Emscripten (emsdk, about 1 GB)
and LVGL 8.4.0 into `simulator/.tools` and `simulator/.deps` (both ignored
by git).

```bash
cd simulator
./build.sh      # compiles to web/datapad.js + web/datapad.wasm
./serve.sh      # http://localhost:8080/
```

Later builds only recompile what changed. After editing the firmware's UI
code, run `./build.sh` again and reload the page.

## GitHub Pages

`.github/workflows/pages.yml` builds the simulator on every push that
touches `firmware/` or `simulator/` and deploys `web/` (HTML + wasm +
sounds, all static) to GitHub Pages. One-time setup: make the repository
public, then in **Settings → Pages** set **Source** to "GitHub Actions".
The site lands at `https://<user>.github.io/<repo>/`. The workflow pins a
known Emscripten version via `setup-emsdk` instead of downloading a fresh
emsdk.

## How it works

| Piece | Where |
| --- | --- |
| UI, i18n, settings, `main.cpp` (setup/loop) | `../firmware/src`, `../firmware/include` (unchanged) |
| Arduino stand-ins (`millis`, `Serial`, `Preferences`, ...) | `shim/` and `src/sim_support.cpp` |
| Board layer: framebuffer, pointer, brightness, LED, panel power | `src/board_sim.cpp` (`BOARD_SIM` in `board.h`) |
| Sound engine | `src/audio_sim.cpp` + `web/sim.js` |
| Link | `src/link_sim.cpp` + `web/sim.js` |
| Page and glue | `web/index.html`, `web/sim.js` |

The firmware loop is driven by a timer (`sim_tick`), not by
`requestAnimationFrame`, because browsers throttle the latter freely.

## Limits

It is a UI simulator, not an ESP32 emulator: speed is not representative of
the hardware, and there is no battery sensor, real deep sleep or real
radio. Anything outside the UI code (`src/audio.cpp`, `src/link.cpp`, the
`board_es3c28p.cpp` layer) is replaced here, so changes to those files are
not exercised by the simulator.
