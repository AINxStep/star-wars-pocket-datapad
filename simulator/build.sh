#!/bin/sh
# Builds the web simulator (simulator/web/datapad.js + datapad.wasm).
# First run downloads Emscripten (emsdk, ~1 GB) and LVGL 8.4.0 into simulator/.
# If emcc is already on PATH (e.g. CI's setup-emsdk), it is used as-is.
set -e
cd "$(dirname "$0")"

if ! command -v emcc >/dev/null 2>&1; then
    # emsdk needs Python >= 3.10; PlatformIO ships one
    if [ -z "$EMSDK_PYTHON" ] && [ -x "$HOME/.platformio/penv/bin/python3" ]; then
        export EMSDK_PYTHON="$HOME/.platformio/penv/bin/python3"
    fi
    if [ ! -d .tools/emsdk ]; then
        mkdir -p .tools
        git clone --depth 1 https://github.com/emscripten-core/emsdk.git .tools/emsdk
    fi
    if [ ! -d .tools/emsdk/upstream/emscripten ]; then
        # Pinned to the same version the Pages workflow uses (.github/workflows/pages.yml)
        (cd .tools/emsdk && ./emsdk install 6.0.11 && ./emsdk activate 6.0.11)
    fi
    . .tools/emsdk/emsdk_env.sh >/dev/null 2>&1
fi

if [ ! -d .deps/lvgl-8.4.0 ]; then
    mkdir -p .deps
    curl -fsSL https://github.com/lvgl/lvgl/archive/refs/tags/v8.4.0.tar.gz | tar xz -C .deps
fi

# Sound clips are served next to the page
mkdir -p web/sounds
cp ../firmware/sounds/*.wav web/sounds/

make -j"$(sysctl -n hw.ncpu 2>/dev/null || nproc)" "$@"
echo "Built simulator/web/datapad.js. Run ./serve.sh and open http://localhost:8080/"
