#!/bin/sh
# Experimental keyboard candidate; never overwrites the verified reference build.
set -eu
export EXTRA_OS_CMAKE_ARGS="${EXTRA_OS_CMAKE_ARGS:-} -DBOOT_STATUS_TRIAL=ON -DBT_KEYBOARD_CLASSIC=ON -DBT_KEYBOARD_DISCOVERY=ON"
exec sh "$(dirname "$0")/build-experimental-four.sh" "${1:-build-keyboard-all}"
