# Star Wars Pocket Datapad

[English](README.md)

Un **datapad / comlink** de bolsillo estilo Star Wars construido alrededor de la
**LCDWIKI ES3C28P** (ESP32-S3 con pantalla táctil IPS capacitiva de 2.8",
códec de audio, amplificador, ranura microSD, cargador de batería y USB-C):
una carcasa paramétrica imprimible en 3D más un firmware táctil con radar
táctico, tabla de sonidos comlink, proyector de hologramas, teclado Aurebesh
e idioma de interfaz completamente intercambiable (español, inglés o Aurebesh).

![El datapad terminado](docs/images/datapad-cover.jpg)

> **Proyecto de fans no oficial.** Star Wars y todos los nombres y diseños
> relacionados pertenecen a Lucasfilm / Disney. Sin ánimo de lucro; ver
> [NOTICE.md](NOTICE.md).

## Aspectos destacados

**Firmware** (`firmware/`, LVGL 8.4 + LovyanGFX, PlatformIO)

- **Radar táctico**: esfera wireframe 3D holográfica que se gira arrastrando,
  con meridiano de barrido y estela que se desvanece; contactos aliados y
  hostiles vuelan dentro y se iluminan cuando el haz los cruza.
- **Proyector de hologramas**: diez modelos wireframe 3D en vivo (Estrella de
  la Muerte, X-Wing, caza TIE, emblema Rebelde, Tatooine, Coruscant, Endor,
  Destructor Estelar, R2-D2, Hoth) con aspecto holográfico (líneas atenuadas
  por profundidad, parpadeo, glitches, líneas de barrido, base de proyector).
  Arrastra para girar, desliza lateralmente para cambiar de modelo.
- **Tabla de sonidos comlink** y sonidos de la interfaz reproducidos por el
  altavoz integrado mediante un motor PCM no bloqueante (códec ES8311 +
  amplificador FM8002E). Los sonidos son archivos WAV cargados desde la
  microSD, la partición LittleFS interna o versiones sintetizadas integradas.
- **Teclado Aurebesh**: toca letras para ver el glifo, su nombre y componer
  texto.
- **Carta estelar**: galaxia espiral estilizada con doce sistemas táctiles;
  los planetas con modelo wireframe saltan directo al proyector de
  hologramas.
- **Baliza de señal**: envía el mensaje del teclado (o "SOS") en código morse
  por el LED RGB más un estroboscopio opcional a pantalla completa.
- **Idiomas**: español, inglés y **Aurebesh** (texto en inglés dibujado con la
  fuente Aurebesh), seleccionable en Ajustes.
- **Temas de color**: Tatooine (por defecto, ámbar y turquesa) e Imperial
  (rojo y gris acero), seleccionable en Ajustes.
- **Enlace P2P (ESP-NOW)**: dos datapads pueden detectarse entre sí por una
  baliza de radio, activable desde la pantalla Comlink.
- **Gestos, monitor de batería y ahorro de energía**: un deslizamiento a la
  derecha vuelve a Inicio, la barra superior muestra la carga real de la
  batería, la pantalla se apaga tras 3 min sin tocarla y la placa entra en
  deep sleep a los 60 min (un toque en la pantalla la despierta).
- Pantalla de arranque, transiciones y ajustes persistentes (brillo, volumen,
  idioma, tema).

**Carcasa** (`3DPrints/`, `docs/`)

- Caja de dos piezas (bisel frontal y chasis trasero), 61 x 120 x 22 mm,
  imprimible sin soportes, con aberturas para USB-C, ranura microSD, botón
  BOOT y una bahía de altavoz para el de fábrica o uno opcional de 20 mm
  (placa adaptadora incluida).
- Incrustaciones del emblema Rebelde y el borde listas para multicolor en el
  bisel frontal.
- Un soporte de escritorio a juego de dos piezas: ensamble de espiga/caja sin
  herramientas, ángulo de visión de 70° y canal continuo de carga USB-C.
- Planos de fabricación (láminas estilo blueprint ISO) en
  `docs/blueprints/` ([docs/enclosure.es.md](docs/enclosure.es.md)).

## Qué necesitas

- **LCDWIKI ES3C28P** — placa ESP32-S3 todo en uno: pantalla IPS ILI9341V de
  2.8" 240x320 (SPI), táctil capacitiva FT6336G, códec de audio ES8311,
  amplificador FM8002E con altavoz, LED RGB WS2812, ranura microSD (SDIO),
  cargador Li-Ion TP4054, ADC de batería y USB-C. Sin cableado.
- Una batería Li-Ion/LiPo de 3.7 V con conector JST PH 2.0 (la placa la carga
  por USB-C; el datapad también funciona solo con alimentación USB).
- Una tarjeta microSD (opcional, para efectos de sonido personalizados).
- La carcasa impresa en 3D: el proyecto listo para laminar es
  [`3DPrints/datapad/ES3C28P_StarWars_Datapad.3mf`](3DPrints/datapad/ES3C28P_StarWars_Datapad.3mf)
  (tres placas: bisel frontal, chasis trasero, montura de altavoz opcional).

## Inicio rápido

```bash
cd firmware
pio run -e esp32-s3-es3c28p -t upload --upload-port /dev/cu.usbmodemXXXX
```

Pasa siempre `--upload-port` explícitamente. Ver
[`firmware/README.es.md`](firmware/README.es.md) para la guía completa de
compilación, la configuración de los clips de sonido (`uploadfs` / microSD)
y el diagnóstico por serie.

## Simulador web

El código real de la interfaz del datapad también compila a WebAssembly, así
que puedes probarlo en el navegador — mismas pantallas, sonidos y enlace P2P
(entre dos pestañas):

- **Demo en vivo**: <https://ainxstep.github.io/star-wars-pocket-datapad/>
- Compílalo localmente con `simulator/build.sh` y sírvelo con
  `simulator/serve.sh` (necesita Emscripten; ver
  [`simulator/README.es.md`](simulator/README.es.md)).

## Estructura del repositorio

```
firmware/          Proyecto PlatformIO (entorno: esp32-s3-es3c28p)
  src/             UI, pantallas, motor de audio, enlace ESP-NOW, capa de placa
  include/         Abstracción board.h, config LVGL, assets generados
  sounds/          Efectos de sonido de muestra (WAV)
  tools/           Generadores de assets (logo, emblema, fuentes, sonidos)
simulator/         Build WebAssembly de la UI real del firmware (Emscripten)
  web/             La página desplegable (index.html, sim.js, datapad.wasm)
3DPrints/
  datapad/         Carcasa: proyecto 3MF, STLs, STEPs, ensamble
  stand/           Soporte de escritorio: STLs y STEPs
docs/
  enclosure.es.md  Dimensiones, tolerancias y ensamble de la carcasa
  blueprints/      Planos de ingeniería (4 láminas, PDF + PNG)
  images/          Fotos y renders
.github/workflows/ Despliegue de GitHub Pages (compila el simulador en CI)
```

## Licencias

- **Código del firmware y del simulador** (`firmware/`, `simulator/`): MIT —
  [LICENSE](LICENSE).
- **Modelos 3D, planos y archivos de carcasa** (`3DPrints/`,
  `docs/blueprints/`, `docs/images/`): CC BY-NC-SA 4.0 —
  [LICENSE-HARDWARE.md](LICENSE-HARDWARE.md).
- Créditos, fuentes y notas de marca registrada: [NOTICE.md](NOTICE.md).
