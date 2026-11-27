#!/bin/sh
set -eu
cd "$(dirname "$0")"
: "${PICO_SDK_PATH:?Set PICO_SDK_PATH to Pico SDK 2.2.0 with TinyUSB}"
work=${1:-build}
cmake -S . -B "$work" -DCMAKE_BUILD_TYPE=Release
cmake --build "$work" --parallel
picotool uf2 convert "$work/retro8bit_bridge_probe.elf" \
    "$work/retro8bit_presto_probe.uf2" --family absolute
