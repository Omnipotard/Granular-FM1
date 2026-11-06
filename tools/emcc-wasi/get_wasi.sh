#!/bin/sh
# SPDX-License-Identifier: MIT
# The WASI sysroot and the wasm32 compiler-rt of wasi-sdk 25 (GitHub releases), for tools/emcc-wasi/emcc.
set -e
WASI_DIR="${WASI_DIR:-$HOME/.wasi}"
mkdir -p "$WASI_DIR"
cd "$WASI_DIR"
u=https://github.com/WebAssembly/wasi-sdk/releases/download/wasi-sdk-25
curl -fsSL "$u/wasi-sysroot-25.0.tar.gz" | tar -xz
curl -fsSL "$u/libclang_rt.builtins-wasm32-wasi-25.0.tar.gz" | tar -xz
echo "WASI sysroot in $WASI_DIR"
