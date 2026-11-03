#!/bin/sh
# Branded entry point; keeps the tested three-slot packer and flash layout.
set -eu
ROOT=$(cd "$(dirname "$0")/.." && pwd)
OUT=${1:-$ROOT/build-retro}
mkdir -p "$OUT"
OUT=$(cd "$OUT" && pwd)
export OS_CMAKE_ARGS="${OS_CMAKE_ARGS:--DSOUND=ON -DAMBIENT=ON -DNIMBUS_TEST=ON -DWEBDIAG=OFF -DC64_PROFILE=OFF}"
sh "$ROOT/scripts/build-dual-boot.sh" "$OUT"
cp "$OUT/presto_dual_boot.uf2" "$OUT/rp2350-8-bit-retro-presto.uf2"
printf 'Firmware: %s\n' "$OUT/rp2350-8-bit-retro-presto.uf2"
