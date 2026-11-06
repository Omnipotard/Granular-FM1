#!/bin/sh
# SPDX-License-Identifier: MIT
# Build GRAINE with fm1-base (base/, a git submodule):  git submodule update --init --recursive
#   ./build.sh                  the FM-1 package (app/build/graine.fwsc): needs the JieLi toolchain and the AC79 SDK
#   ./build.sh --release X.Y    a numbered release (graine-X.Y.fwsc)
#   ./build.sh --host           the simulator (app/build/host/graine_host): any C compiler
#   ./build.sh --web            the browser emulator (app/build/emu/site/ and app/build/emu/graine-emulateur.html):
#                               Emscripten, or clang + the WASI sysroot through tools/emcc-wasi/emcc (see CLAUDE.md)
#   JIELI_TOOLCHAIN  JieLi Linux toolchain (default: ~/.jieli/toolchain)
#   AC79_SDK         JieLi AC79 SDK checkout (default: ~/fw-AC79_AIoT_SDK)
#   PYTHON           a Python with Pillow (and libraqm, for the fonts)
set -e
cd "$(dirname "$0")"
export JIELI_TOOLCHAIN="${JIELI_TOOLCHAIN:-$HOME/.jieli/toolchain}"
export AC79_SDK="${AC79_SDK:-$HOME/fw-AC79_AIoT_SDK}"
PY="${PYTHON:-python3}"
[ -f base/tools/build.py ] || { echo "build.sh: no fm1-base in base/ (git submodule update --init --recursive)"; exit 1; }
"$PY" -c 'import PIL' 2>/dev/null || { echo "build.sh: $PY has no Pillow (pip3 install Pillow)"; exit 1; }
[ -f app/sounds/bank.h ] || "$PY" tools/gen_sounds.py
case " $* " in
    *" --web "*)
        "$PY" base/tools/build.py app "$@"
        exec "$PY" tools/emu_page.py;;
esac
exec "$PY" base/tools/build.py app "$@"
