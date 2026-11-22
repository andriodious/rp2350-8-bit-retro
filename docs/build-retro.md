# Building RP2350 8-bit Retro

Use a fresh checkout/extracted source directory. Existing test builds remain
useful rollback firmware, but their CMake caches refer to their original paths.

```sh
scripts/setup.sh --no-build --roms
. deps/env.sh
EXTRA_OS_CMAKE_ARGS='-DC64_ABORT_DIAG=ON' scripts/build-classic-keyboard-four.sh build-release
```

On Debian/Raspberry Pi OS, `scripts/setup.sh --apt --no-build --roms` can install
host packages. Setup fetches the pinned Arm toolchain and Pico SDK 2.2.0, and BBC
assets. C64 configuration runs `scripts/fetch-c64.py`, which fetches the pinned
FRANK fork, applies the recorded adaptation and verifies hashes, then downloads
and verifies the configured C64 Elite disk. See `third_party/c64-sources.json`.

The output is `build-release/rp2350-8-bit-retro-presto.uf2`, a combined launcher,
Master, Model B, C64 and Spectrum 48K image. The intermediate `presto_dual_boot.uf2` is identical;
the old internal filename and partition identifiers remain for compatibility.
No separate C64 fork edit or moving-branch dependency is needed for this release.

Default release features: BT_KEYBOARD_CLASSIC=ON, BT_KEYBOARD_DISCOVERY=ON, BOOT_STATUS_TRIAL=ON, SOUND=ON, AMBIENT=ON, NIMBUS_TEST=ON, WEBDIAG=OFF and
C64_PROFILE=OFF. The release command also sets C64_ABORT_DIAG=ON to retain the
terminal-caller capture used by the hardware-tested firmware; the underlying CMake
default remains OFF. The four C64 optimisations remain enabled by default. NIMBUS_TEST
is a legacy build-option name; it enables the complete original Nimbus support.
For an independent build with web diagnostics, set `EXTRA_OS_CMAKE_ARGS="-DC64_ABORT_DIAG=ON -DWEBDIAG=ON"` when calling the keyboard build script. For a custom build without Bluetooth, use the lower-level CMake options with NIMBUS_TEST=OFF, BT_KEYBOARD_CLASSIC=OFF and BT_KEYBOARD_DISCOVERY=OFF; the release wrapper deliberately enables Bluetooth.

Pass NINJA_JOBS=2 on a memory-constrained host. `SDK_CMAKE_ARGS` remains available
for platform-specific SDK tools. Existing `scripts/build-dual-boot.sh` and
single-machine CMake targets remain valid. The inherited setup command without
`--no-build` still uses the legacy combined-build output path; the commands above
are the recommended branded entry point.

The clean firmware has sound controls but hides test traces and raw controller
readouts. Controller pairing, maps and reconnect remain visible. Existing saved
pairing, Wi-Fi and disk-save locations are unchanged. Clock frequency is 250 MHz.

## v0.9.0 four-machine release

The release uses the verified checkpoint `reference/four-machine-startup-dma-20261122`.
The historical `build-experimental-four.sh` name also supplies the release's
D81 performance options. `BOOT_STATUS_TRIAL=ON` enables the verified paused SD
chooser, touchscreen keyboard, configuration and status screens, including Bluetooth setup. Keep APP and SAVE unset to reproduce this release.
Spectrum sources in `src/cores/chips` match the pinned fork recorded in `UPSTREAM`;
`scripts/fetch-spectrum.py` downloads and verifies Elite against
`test-assets/elite48/provenance.json`. Game images are not committed to Git.

Build IDs and source labels can differ on rebuild. The downloadable release UF2
is the exact hardware-tested checkpoint, with its original component IDs and
source label. See the release checksum manifest for identification.

Classic HID keyboard support is enabled; BLE-only keyboards are not supported.
The exact release firmware is retained from source commit
`9f07f635947b4aa80944375154ca32043f04a220`, with C64 terminal-caller capture enabled.
Its combined UF2 SHA256 is
`b761a3a64304b67294fb6fcacea3784d83c45539a118e804a5068db37e91c14d`.
Clean source rebuilds and CI artifacts verify the build separately; they do not
replace the hardware-tested release binary and are not automatically hardware
verified. Keep the retained matching ELF files when decoding failure records.
v0.8.0 and previous checkpoints remain unchanged. Settings v2 can require
re-pairing when rolling back to older firmware.
