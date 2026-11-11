#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
clang -O2 -g -fsanitize=address,undefined -I src/cores/chips tests/media_test.c src/machines/spectrum/tape.c build/spectrum_assets.c -o /tmp/presto-spectrum-media-test
/tmp/presto-spectrum-media-test
