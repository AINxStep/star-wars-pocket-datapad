# Notices and credits

## Trademarks and fan-project disclaimer

STAR WARS, the Rebel Alliance emblem, Aurebesh references and all related names,
characters, vehicles and designs are trademarks and/or copyrighted works of
Lucasfilm Ltd. / The Walt Disney Company. This is an **unofficial, non-commercial
fan project**. It is not affiliated with, endorsed or sponsored by Lucasfilm or
Disney, and it is not intended for sale. The sound effects included here were
synthesized from scratch (`firmware/tools/gen_sounds.py`) and are not taken from
any film or game. The three ambient-chatter slots (`coms1-3.wav`) ship as one
second of silence: the project contains no third-party audio. If you put your
own clips there, do not commit or redistribute audio you do not have the rights to.

## Third-party components

| Component | Use | License |
| --- | --- | --- |
| [LVGL](https://lvgl.io/) 8.3/8.4 | UI library | MIT |
| [LovyanGFX](https://github.com/lovyan03/LovyanGFX) | Display driver | FreeBSD (BSD 2-Clause) |
| [XPT2046_Touchscreen](https://github.com/PaulStoffregen/XPT2046_Touchscreen) | Touch controller driver | MIT-style (see repository) |
| [PlatformIO](https://platformio.org/) / [Arduino-ESP32](https://github.com/espressif/arduino-esp32) | Build system and framework | Apache-2.0 / LGPL-2.1 |
| [Aurebesh AF](https://aurekfonts.github.io/) by AurekFonts | Glyphs converted into the LVGL bitmap fonts in `firmware/src/ui/font_aurebesh_*.c` | Free for all personal and commercial uses (per the author's page); attribution appreciated |
| [lv_font_conv](https://github.com/lvgl/lv_font_conv) | Font conversion (build-time tool, not distributed) | MIT |

## Rebel Alliance crest artwork

The crest artwork used on the enclosure and in the firmware (`include/rebel_logo.h`,
`include/holo_crest.h`) is the author's own faithful vectorization of the Rebel
Alliance emblem, redrawn in Inkscape (the tracked `starwars-rebel-svgrepo-com.svg`
was the tracing reference). The emblem design itself remains a Lucasfilm Ltd. /
Disney trademark — see the fan-project disclaimer above.

## Contributors

Created by AINxStep.