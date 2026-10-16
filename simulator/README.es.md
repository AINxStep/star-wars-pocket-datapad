# Simulador web

Ejecuta la interfaz real del datapad en un navegador, en local. El propio
código de la UI del firmware (pantallas, textos, temas, fuentes Aurebesh,
el rasterizador vectorial) se compila a WebAssembly con
[Emscripten](https://emscripten.org/) junto con LVGL 8.4; solo la capa de
hardware se sustituye por una del navegador.

- El formato es el del hardware: **ES3C28P**, 240x320 en vertical.
- El ratón (o un dedo) es la pantalla táctil; arrastra para girar los
  hologramas y el radar, desliza a la derecha en la barra superior para
  volver.
- Sonido por la Web Audio API, con los mismos clips que el firmware.
- El LED de estado y el brillo de la pantalla (y el apagado por inactividad)
  se dibujan en la página. La salida serie del firmware aparece en el panel
  lateral.
- Los ajustes (idioma, tema, volumen, ...) persisten en el `localStorage`
  del navegador.
- **Enlace P2P entre dos datapads:** pulsa "Segundo datapad" para abrir otra
  pestaña, activa P2P en Comlink en ambas y envía sonidos entre ellas (un
  `BroadcastChannel` sustituye a ESP-NOW, solo en el mismo navegador).

## Compilar y ejecutar

Necesita `git`, `curl`, `make` y un Python 3.10+ (se usa el propio Python
de PlatformIO si está presente). La primera compilación descarga Emscripten
(emsdk, ~1 GB) y LVGL 8.4.0 en `simulator/.tools` y `simulator/.deps`
(ambos ignorados por git).

```bash
cd simulator
./build.sh      # compila a web/datapad.js + web/datapad.wasm
./serve.sh      # http://localhost:8080/
```

Las compilaciones posteriores solo recompilan lo que cambió. Tras editar el
código de la UI del firmware, ejecuta `./build.sh` de nuevo y recarga la
página.

## GitHub Pages

`.github/workflows/pages.yml` compila el simulador en cada push que toca
`firmware/` o `simulator/` y despliega `web/` (HTML + wasm + sonidos, todo
estático) en GitHub Pages. Configuración única: haz el repositorio público
y en **Settings → Pages** pon **Source** en "GitHub Actions". El sitio
queda en `https://<usuario>.github.io/<repo>/`. El workflow fija una versión
conocida de Emscripten vía `setup-emsdk` en vez de descargar un emsdk nuevo.

## Cómo funciona

| Pieza | Dónde |
| --- | --- |
| UI, i18n, ajustes, `main.cpp` (setup/loop) | `../firmware/src`, `../firmware/include` (sin cambios) |
| Sustitutos de Arduino (`millis`, `Serial`, `Preferences`, ...) | `shim/` y `src/sim_support.cpp` |
| Capa de placa: framebuffer, puntero, brillo, LED, alimentación del panel | `src/board_sim.cpp` (`BOARD_SIM` en `board.h`) |
| Motor de sonido | `src/audio_sim.cpp` + `web/sim.js` |
| Enlace | `src/link_sim.cpp` + `web/sim.js` |
| Página y glue | `web/index.html`, `web/sim.js` |

El bucle del firmware lo gobierna un temporizador (`sim_tick`), no
`requestAnimationFrame`, porque los navegadores limitan este último a su
antojo.

## Límites

Es un simulador de UI, no un emulador de ESP32: la velocidad no es
representativa del hardware, y no hay sensor de batería, deep sleep real ni
radio real. Todo lo que está fuera del código de la UI (`src/audio.cpp`,
`src/link.cpp`, la capa `board_es3c28p.cpp`) se sustituye aquí, así que los
cambios en esos archivos no se ejercitan en el simulador.
