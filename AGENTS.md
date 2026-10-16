# Agent notes — Star Wars Pocket Datapad (public repo)

ES3C28P-only public project. The CAD source models, generators and old test
boards (CYD, S3-EV) live in the private workshop repo — only exported
artifacts (STL/STEP/3MF/blueprints) flow into `3DPrints/` and `docs/` here.

## Layout

- `firmware/` — PlatformIO project, single env `esp32-s3-es3c28p`
  (LVGL 8.4 + LovyanGFX, Arduino framework).
- `simulator/` — WASM build of the real firmware UI (`build.sh`, `serve.sh`;
  port 8080). Emscripten/LVGL caches (`.tools`, `.deps`, `.build`) are
  git-ignored.
- `3DPrints/` — manufacturing assets: `datapad/` (3MF project + STL/STEP) and
  `stand/` (desktop stand). Oversized STLs ship as GitHub Release assets.
- `docs/` — enclosure docs EN/ES, `blueprints/` (4 sheets PDF+PNG), `images/`.
- `.github/workflows/pages.yml` — builds the simulator with emsdk 6.0.11 in CI
  and deploys `simulator/web/` to GitHub Pages.

## Build & verify

```sh
# Firmware (pio is not on PATH)
~/.platformio/penv/bin/pio run -e esp32-s3-es3c28p

# Simulator WASM
cd simulator && ./build.sh && ./serve.sh   # http://localhost:8080
```

Docs-only changes need no rebuild. The Pages workflow only runs when
`firmware/`, `simulator/` or the workflow itself changes.

## Rules

- **Bilingual docs:** every user-facing doc exists in English and Spanish
  (`README.md`/`README.es.md`, `firmware/README*.md`, `simulator/README*.md`,
  `docs/enclosure*.md`). Update both in the same change.
- **Commits:** author `AINxStep <114045473+AINxStep@users.noreply.github.com>`,
  via `git -c user.name=... -c user.email=...` — never touch git config.
  No `Co-Authored-By`, no AI attribution anywhere (repo, NOTICE, PRs).
- **Only ES3C28P** is a supported target. Do not reintroduce CYD/S3-EV as
  public targets. "LCDWIKI" appears only as a documentation source citation
  (IO table, CR2025-MI6140 drawing).
- **Never commit:** `*.FCStd`/`*.FCBak`, CAD generator scripts, `.pio/`,
  `sounds/` copyrighted audio, personal data or local paths. `.3mf` files from
  Bambu Studio must be sanitized first (`DesignerUserId`,
  `print_settings_id`).
- **Audio:** all shipped sounds are original/synthesized (`tools/gen_sounds.py`);
  `coms1-3.wav` are intentional silence (ambient slots). Never commit movie or
  game audio.
- **Photos:** strip JPEG EXIF/APPn/COM metadata before committing.
- **Facts worth knowing:** rear chassis ships in the `.3mf` with supports
  enabled (optional, not required); stock speaker is rectangular but its cable
  is too short — the `Speaker_Mount` plate adapts a generic rectangular
  replacement; battery 2000–3000 mAh; screws 4× M3×17 mm.
