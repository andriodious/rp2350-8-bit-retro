#!/bin/sh
set -eu
# Pass an extracted reference source directory as the first argument.
reference=$1
cd "$(dirname "$0")/.."
clang -O2 -DBASELINE -I "$reference/src/machines/spectrum" -I "$reference/src/cores/chips" tests/spectrum_compare.c build/spectrum_assets.c -o /tmp/spectrum-compare-base
clang -O2 -I src/machines/spectrum -I src/cores/chips tests/spectrum_compare.c src/machines/spectrum/tape.c build/spectrum_assets.c -o /tmp/spectrum-compare-new
/tmp/spectrum-compare-base > /tmp/spectrum-base.frames
/tmp/spectrum-compare-new > /tmp/spectrum-new.frames
diff -u /tmp/spectrum-base.frames /tmp/spectrum-new.frames
