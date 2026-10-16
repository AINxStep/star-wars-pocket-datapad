# Firmware del datapad (ES3C28P)

[English](README.md) · [Vista general del proyecto](../README.es.md)

Firmware de pantalla táctil para el datapad de bolsillo de Star Wars,
escrito para el framework Arduino con **LVGL 8.4** (UI) y **LovyanGFX**
(pantalla), compilado con **PlatformIO**.

## Funciones

- **Radar táctico**: esfera wireframe 3D holográfica (arrastra para girar)
  con meridiano de barrido y estela que se desvanece; contactos aliados y
  hostiles vuelan en 3D y se revelan al pasar el haz; tarjeta de información
  (hostil más cercano, distancia, rumbo).
- **Proyector de hologramas**: diez modelos wireframe 3D generados en código
  (Estrella de la Muerte, X-Wing, TIE/LN, emblema Rebelde, Tatooine,
  Coruscant, Endor, Destructor Estelar, R2-D2, Hoth) con líneas atenuadas
  por profundidad, parpadeo, glitches, líneas de barrido y base de
  proyector. Arrastra para girar, flechas o deslizamiento lateral para
  cambiar de modelo.
- **Tabla de sonidos comlink**: R2-D2, bláster, canal comlink y alarma
  imperial.
- **Enlace P2P (ESP-NOW)**: enlace de radio opcional entre dos datapads.
  Se activa desde la pantalla Comlink; una baliza a 1 Hz anuncia el
  dispositivo y el cuadro de estado muestra LINK OFF / SEARCHING / PEER OK.
  Con TX activo, cada efecto de la tabla de sonidos se envía también al
  compañero, que lo reproduce y muestra un aviso de "mensaje recibido"
  (la alarma en color de alerta).
- **Teclado Aurebesh**: teclas de glifos, tarjeta de la letra seleccionada
  (glifo, letra latina, nombre) y una línea de mensaje que alimenta la
  baliza de señal.
- **Carta estelar**: galaxia espiral estilizada con doce sistemas táctiles
  y tarjeta de información por sistema; los planetas con modelo wireframe
  ofrecen un atajo HOLO que salta directo al proyector (Alderaan abre la
  Estrella de la Muerte).
- **Baliza de señal**: transmite el mensaje compuesto en el teclado Aurebesh
  (o "SOS" si está vacío) en código morse — destellos del LED RGB más un
  estroboscopio opcional a pantalla completa.
- **Idioma de la interfaz**: español, inglés o Aurebesh (texto en inglés
  dibujado con la fuente Aurebesh), seleccionable en Ajustes y guardado en
  flash.
- **Ajustes**: brillo, volumen, idioma, tema de color (Tatooine o Imperial)
  y ahorro de energía.
- **Gestos**: un deslizamiento rápido a la derecha en la barra superior
  vuelve a Inicio; un deslizamiento lateral en el holograma cambia de
  modelo.
- **Monitor de batería**: la placa trae de fábrica cargador TP4054 y un
  divisor de batería, así que la barra superior muestra el porcentaje real.
- **Ahorro de energía**: con el interruptor AHORRO activo, la pantalla se
  apaga tras 3 min sin tocarla; a los 60 min la placa entra en deep sleep
  y un toque en la pantalla la reinicia, como un interruptor de encendido.
  El toque que despierta la pantalla solo la despierta — nunca activa lo
  que hay debajo.
- Pantalla de arranque, transiciones y retroalimentación con LED RGB WS2812.

## Hardware y mapa de pines

Placa: **ES3C28P** — ESP32-S3 con pantalla IPS ILI9341V de 2.8"
240x320 (SPI), táctil capacitiva FT6336G, códec de audio ES8311,
amplificador FM8002E con altavoz, LED RGB WS2812, ranura microSD (SDIO),
cargador Li-Ion TP4054 con medición de batería y USB-C. La pantalla va en
vertical para que el USB-C quede abajo, a juego con la carcasa.

| Función | Pines |
| --- | --- |
| Pantalla (SPI, 40 MHz) | SCK 12, MOSI 11, MISO 13, CS 10, DC 46, luz de fondo 45 (activa en alto) |
| Táctil FT6336G (I2C) | SDA 16, SCL 15, RST 18, INT 17 |
| Códec ES8311 (I2S) | MCLK 4, BCLK 5, WS 7, DOUT 8; I2C 0x18 en el bus del táctil |
| Amplificador FM8002E | Habilitación GPIO 1 (activo en bajo) |
| LED RGB (WS2812) | GPIO 42 |
| Medición de batería | GPIO 9 (ADC1_CH8, divisor 1/2) |
| microSD (SDIO 4 bits) | CLK 38, CMD 40, D0 39, D1 41, D2 48, D3 47 |
| Botón BOOT | GPIO 0 |
| Carga / flasheado | USB-C (USB nativo, TinyUSB) |

## Compilación y flasheado

Solo necesitas un ordenador, un cable USB-C y
[PlatformIO](https://platformio.org/) — no hace falta experiencia con
ESP32. Elige **una** de las dos opciones.

### Opción A — VS Code + PlatformIO (la más fácil, gráfica)

1. Instala [Visual Studio Code](https://code.visualstudio.com/), ábrelo e
   instala la extensión **PlatformIO IDE** (panel de Extensiones, busca
   "PlatformIO IDE"). Reinicia VS Code cuando lo pida.
2. Descarga este repositorio: botón verde **Code → Download ZIP** en
   GitHub y descomprime el archivo. (O `git clone` si ya usas git.)
3. En VS Code, **File → Open Folder…** y abre la carpeta `firmware/` del
   proyecto descomprimido (la que contiene `platformio.ini`).
4. Conecta la ES3C28P al ordenador con el cable USB-C.
5. Pulsa el icono de alienígena **PlatformIO** en la barra lateral y, bajo
   **PROJECT TASKS → esp32-s3-es3c28p → General**, haz clic en **Upload**.
   La primera compilación descarga el toolchain y las librerías (tarda
   unos minutos); las siguientes tardan segundos.
6. Listo — el datapad se reinicia en la pantalla de arranque cuando
   termina la carga.

Si la carga falla con "could not open port" o "no serial data received",
la placa no está en modo descarga. Entra manualmente: mantén pulsado el
botón **BOOT** (GPIO 0, serigrafiado en la placa), pulsa **RST** y suelta
BOOT — y vuelve a pulsar **Upload**.

### Opción B — PlatformIO Core (línea de comandos)

1. Instala [PlatformIO Core](https://docs.platformio.org/en/latest/core/installation/index.html).
2. Abre un terminal en la carpeta `firmware/` y ejecuta:

   ```bash
   pio run -e esp32-s3-es3c28p -t upload --upload-port /dev/cu.usbmodemXXXX
   ```

   Encuentra tu puerto con `pio device list` — la ES3C28P aparece como
   `cu.usbmodem*` (macOS/Linux) o `COMx` (Windows). Pasa siempre
   `--upload-port` explícitamente — la autodetección puede elegir la placa
   equivocada.
3. Mismo plan B si falla la carga: modo descarga manual
   (mantén **BOOT** → pulsa **RST** → suelta **BOOT**).

El proyecto usa la tabla de particiones `huge_app` (3 MB para el programa)
y guarda los ajustes en NVS — al reflashear se conservan brillo, volumen
e idioma.

### Diagnóstico por serie

TinyUSB descarta los prints de arranque mientras ningún host mantenga el
puerto abierto. Al conectar un monitor serie, `loop()` reimprime una vez el
informe de carga de audio (`[AUDIO] source … clips`) y el estado de la
batería (`[BATT]`) cada 10 s:

```bash
pio device monitor -b 115200 -p /dev/cu.usbmodemXXXX
```

En deep sleep la consola USB desaparece; conectar un monitor reinicia la
placa, así que flashea solo estando despierta (o en modo descarga manual).

## Sonidos personalizados (opcional)

El datapad funciona de fábrica con efectos sintetizados integrados — no
necesitas tarjeta microSD ni archivos de audio. Si quieres clips reales
(frases de las películas, pitidos auténticos de R2-D2, etc.), puedes
sustituir cada sonido copiando tus propios WAV a una microSD.

### Cómo funciona

Cada sonido tiene un nombre de archivo fijo. Al arrancar, el firmware
monta **una** fuente de sonido — la tarjeta microSD si hay una insertada;
si no, la partición LittleFS interna. Cualquier archivo que falte en esa
fuente (o un WAV que no se pueda leer) usa el clip sintetizado compilado
en el firmware, así que el datapad siempre tiene todos los sonidos.

### Sustituir sonidos con los tuyos (microSD — lo más fácil)

1. Formatea una microSD en **FAT32** (el formato por defecto en tarjetas
   de hasta 32 GB).
2. En la raíz de la tarjeta, crea una carpeta llamada exactamente
   `sounds` (en minúsculas).
3. Copia ahí tus archivos WAV, renombrándolos según la tabla. Cualquier
   archivo que no pongas simplemente conserva su sonido sintetizado.

```
raíz de la microSD/
└── sounds/
    ├── click.wav     sonido de toque en la UI
    ├── r2d2.wav      pantalla Comlink — botón R2-D2
    ├── blaster.wav   pantalla Comlink — botón bláster
    ├── comlink.wav   pantalla Comlink — botón de canal
    ├── alarm.wav     pantalla Comlink — botón alarma imperial
    ├── holo.wav      activación del proyector de hologramas
    ├── coms1.wav     ranura ambiental 1 (ver abajo)
    ├── coms2.wav     ranura ambiental 2
    └── coms3.wav     ranura ambiental 3
```

4. Inserta la tarjeta y enciende (o reinicia). El log de serie imprime
   `[AUDIO] microSD mounted` y qué archivos se cargaron desde dónde.

**Formato de archivo:** WAV PCM, 8 o 16 bits, mono o estéreo, 4–48 kHz.
Mantén los efectos por debajo de ~6 s; los clips más largos se guardan a
mitad de frecuencia de muestreo para ahorrar RAM. Usa audio sobre el que
tengas derechos — clips libres de regalías, tus propias grabaciones o
sonidos con licencia.

### Charla ambiental (`coms*.wav`) — qué es

Mientras la pantalla de Inicio está visible, el datapad reproduce al azar
uno de los tres clips `coms` tras un intervalo de 1-5 minutos, como el
ruido de fondo de un comlink en la consola de una nave. Las tres ranuras
se turnan en orden barajado (nunca el mismo clip dos veces seguidas) y se
salta si otro efecto está sonando.

El repositorio incluye `coms1/2/3.wav` como **un segundo de silencio**,
así que el dispositivo está completamente callado hasta que pongas tus
propios archivos — solo reemplázalos si quieres el efecto ambiental.

### Alternativa: flash interna (LittleFS)

Solo se usa cuando **no hay tarjeta microSD insertada**. Si prefieres no
usar tarjeta, pon los WAV en la carpeta `firmware/sounds/` (mismos
nombres) y súbelos a la partición LittleFS de la placa:

```bash
pio run -e esp32-s3-es3c28p -t uploadfs --upload-port /dev/cu.usbmodemXXXX
```

(En VS Code: **PROJECT TASKS → esp32-s3-es3c28p → Platform → Upload
Filesystem Image**.) La ruta de la microSD es más sencilla — es solo
copiar archivos.

Los efectos de muestra en `sounds/` son los mismos originales
sintetizados que usa el firmware como respaldo; regenéralos con
`python3 tools/gen_sounds.py`.

## Temas de color

Dos temas, seleccionables en **Ajustes** (TATOOINE / IMPERIAL) y guardados
en flash; Tatooine es el predeterminado. Cambiarlo reconstruye todas las
pantallas, igual que el idioma.

| Tema | Aspecto |
| :--- | :--- |
| **Tatooine** | Marrón desierto oscuro, acento ámbar de soles gemelos, texto secundario turquesa, alertas rojas, hologramas azules clásicos |
| **Imperial** | Acero casi negro, acento rojo imperial, texto secundario gris hielo, alertas ámbar (para destacar sobre el acento rojo), hologramas azul hielo |

Los colores se definen por rol en `struct UiTheme` (`src/ui/ui_common.h`) y
se leen con las macros `COLOR_*`: `ACCENT` (bordes, títulos, controles
seleccionados), `DATA` (etiquetas secundarias), `ALERT` (contactos
hostiles, alerta roja), `FRIEND` (contactos aliados), `TEXT`/`BG`/`PANEL` y
`HOLO`/`CHAMBER`/`HILITE` para las proyecciones de holograma y radar. Usa
estos roles en vez de colores literales en código UI nuevo. Para añadir un
tema: añade una entrada `THEME_*` al enum, una fila a la tabla `themes[]`,
sube el `constrain()` en `settings.h` y añade un botón en
`add_theme_selector()` (`screens/settings.cpp`).

## Estructura del código

```
src/main.cpp             Setup, glue de LVGL, entrada táctil, sleep de pantalla,
                         deep sleep, bucle principal
src/audio.cpp            Motor de sonido PCM (códec ES8311 + carga WAV de SD/LittleFS)
src/link.cpp             Enlace de balizas ESP-NOW entre datapads
src/board_es3c28p.cpp    Capa de placa: panel, táctil, luz de fondo, alimentación
                         del códec, deep sleep
src/i18n.cpp             Cadenas traducidas (ES / EN / Aurebesh)
src/ui/ui.cpp            Ciclo de vida de la UI, navegación, reconstrucción por idioma
src/ui/ui_common.*       Tema, estilos, barra superior, fuentes, registro de pantallas
src/ui/screens/*.cpp     Un archivo por pantalla: splash, home, radar, holo (+ modelos),
                         comlink, aurebesh, settings, calibration, starchart, beacon
include/                 Abstracción board.h, helpers de hardware, ajustes, config
                         LVGL, datos generados
tools/                   Generadores (logo, emblema del holograma, sonidos, fuentes
                         Aurebesh)
sounds/                  Efectos de sonido de muestra generados
```

**Añadir una pantalla:** crea `src/ui/screens/<nombre>.cpp` exponiendo
`lv_obj_t* screen_<nombre>_create(void)`, añádela a `enum UiScreen`
(`ui_common.h`) y a `create_screens()` en `ui.cpp`, y navega con
`ui_goto(SCR_<NOMBRE>)`. Los temporizadores de una pantalla deben crearse
una sola vez (las pantallas se reconstruyen al cambiar de idioma).

**Añadir o cambiar texto:** añade un `StrId` en `include/i18n.h` y una fila
en la tabla de `src/i18n.cpp` (español, inglés y una redacción Aurebesh
opcional más corta, ya que los glifos Aurebesh son ~1.7x más anchos que las
letras latinas). Usa `tr(S_...)` para textos y `ui_font(size)` para fuentes.

## Assets generados y herramientas

| Herramienta | Produce |
| --- | --- |
| `tools/gen_logo.py` | `include/rebel_logo.h`: bitmap del logo del arranque, rasterizado desde el SVG |
| `tools/gen_holo_crest.py` | `include/holo_crest.h`: contorno simplificado del emblema para el holograma |
| `tools/gen_sounds.py` | `sounds/*.wav`: efectos de sonido sintetizados |
| `tools/gen_aurebesh_font.sh` | `src/ui/font_aurebesh_*.c` e `include/aurebesh_font.h` (necesita Node.js y un archivo de fuente Aurebesh) |

Las fuentes Aurebesh están commiteadas, así que solo necesitas el script
para cambiar tamaños o la fuente origen. Sin las fuentes la UI compila igual
y muestra letras latinas. Los glifos vienen de **Aurebesh AF** (ver
[NOTICE](../NOTICE.md)).

## Notas técnicas

- **Audio por el códec ES8311.** Una tarea anclada al núcleo 1 (prioridad 5,
  por encima de `loop()` para que las tareas del sistema no la ahoguen)
  envía PCM a 16 kHz por I2S: MCLK 4, BCLK 5, WS 7, DOUT 8. El códec está en
  I2C 0x18 en el bus del táctil (SCL 15 / SDA 16, propiedad de `Wire`), y el
  amplificador FM8002E se habilita con GPIO 1 (activo en bajo). Los
  underruns repiten el último buffer en vez de escribir ceros.
- **Clips de sonido en PSRAM.** Los WAV decodificados van a
  `MALLOC_CAP_SPIRAM` para que los buffers de render de la pantalla conserven
  la RAM interna; la charla ambiental puede usar el clip completo de 6 s.
- **Deep sleep.** Con el ahorro activado, 3 min sin tocar apagan la pantalla
  (`board_display_power(false)` corta panel y luz de fondo); a los 60 min
  `audio_shutdown()` silencia el códec y bloquea el pin del amplificador con
  un GPIO hold antes de que `board_enter_deep_sleep()` apague el SoC con la
  línea INT del FT6336G (GPIO 17, RTC) como fuente de despertar. **Los GPIO
  holds puestos antes del deep sleep sobreviven al reset de despertar**, así
  que el arranque los libera (`gpio_deep_sleep_hold_dis()` + `gpio_hold_dis()`)
  o la luz de fondo y el amplificador quedan apagados para siempre. Prueba el
  pipeline en banco sin editar código:
  `PLATFORMIO_BUILD_FLAGS="-D SLEEP_TIMEOUT_MS=15000 -D DEEP_SLEEP_TIMEOUT_MS=45000"`.
- **Las escrituras a flash deshabilitan la caché de CPU** (NVS, LittleFS): la
  capa de placa detiene el DMA de la pantalla alrededor de ellas
  (`board_storage_begin/end()`); los clips de sonido se leen al arrancar
  antes de que el panel empiece a transmitir.
- **Memoria.** LVGL tiene un pool de 64 KB (unos 40 KB en uso una vez
  construidas todas las pantallas). Cambiar de idioma reconstruye todas las
  pantallas: las viejas se borran primero porque dos juegos completos no
  caben.
- **Renderizador de hologramas.** Los modelos son listas procedurales de
  vértices/aristas, rotados y proyectados en CPU cada frame y dibujados con
  primitivas de línea de LVGL en un callback de dibujo propio.
- **Enlace ESP-NOW.** Activarlo levanta el driver Wi-Fi en modo estación
  (no hace falta AP) y cuesta unos 40-60 KB de heap. La radio y sus callbacks
  corren en el núcleo 0 mientras la tarea de audio sigue en el núcleo 1. Con
  un solo dispositivo puedes verificar que el enlace arranca y las balizas se
  envían (log de serie); confirmar la detección de pares necesita un segundo
  datapad (o dos pestañas del simulador).

## Resolución de problemas

- **Sin sonido**: revisa el volumen en Ajustes; el log de serie imprime la
  fuente de audio (`[AUDIO] source: …`) al conectar un monitor. Con clips en
  microSD, verifica que estén en `/sounds/` y sean WAV PCM.
- **El flasheado falla**: pasa `--upload-port` explícitamente, o entra en
  modo descarga (mantén BOOT → pulsa RST → suelta BOOT).
- **Pantalla o amplificador muertos tras despertar del deep sleep**: los
  GPIO holds no se liberaron en el arranque — ver la nota de deep sleep.
- **La serie no muestra nada**: abre el puerto después del arranque
  (TinyUSB descarta los prints tempranos); el firmware reimprime el informe
  de audio y empieza los logs `[BATT]` cuando un host se conecta.
