#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
clang -O2 -g -fsanitize=address,undefined -I src/cores/chips tests/spectrum_test.c src/machines/spectrum/tape.c build/spectrum_assets.c -o /tmp/presto-spectrum-test
/tmp/presto-spectrum-test test-assets/elite48/ELITE.SNA
