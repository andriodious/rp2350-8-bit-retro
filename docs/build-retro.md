# Building RP2350 8-bit Retro

Use a fresh checkout/extracted source directory. Existing test builds remain
useful rollback firmware, but their CMake caches refer to their original paths.

```sh
scripts/setup.sh --no-build --roms
. deps/env.sh
scripts/build-classic-keyboard-four.sh build-release
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
C64_PROFILE=OFF. The four C64 optimisations remain enabled by default. NIMBUS_TEST
is a legacy build-option name; it enables the complete original Nimbus support.
For an independent build with web diagnostics, set `EXTRA_OS_CMAKE_ARGS="-DWEBDIAG=ON"` when calling the keyboard build script. For a custom build without Bluetooth, use the lower-level CMake options with NIMBUS_TEST=OFF, BT_KEYBOARD_CLASSIC=OFF and BT_KEYBOARD_DISCOVERY=OFF; the release wrapper deliberately enables Bluetooth.

Pass NINJA_JOBS=2 on a memory-constrained host. `SDK_CMAKE_ARGS` remains available
for platform-specific SDK tools. Existing `scripts/build-dual-boot.sh` and
single-machine CMake targets remain valid. The inherited setup command without
`--no-build` still uses the legacy combined-build output path; the commands above
are the recommended branded entry point.

The clean firmware has sound controls but hides test traces and raw controller
readouts. Controller pairing, maps and reconnect remain visible. Existing saved
pairing, Wi-Fi and disk-save locations are unchanged. Clock frequency is 250 MHz.

## v0.6.0 four-machine release

The release uses the verified checkpoint `reference/four-machine-clean-verified-20261114`.
The historical `build-experimental-four.sh` name also supplies the release's
D81 performance options. `BOOT_STATUS_TRIAL=ON` enables the verified paused SD
chooser and status screen. Keep APP and SAVE unset to reproduce this release.
Spectrum sources in `src/cores/chips` match the pinned fork recorded in `UPSTREAM`;
`scripts/fetch-spectrum.py` downloads and verifies Elite against
`test-assets/elite48/provenance.json`. Game images are not committed to Git.

Build IDs and source labels can differ on rebuild. The downloadable release UF2
is the exact hardware-tested checkpoint, with its original component IDs and
source label. See the release checksum manifest for identification.

Classic HID keyboard support is enabled; BLE-only keyboards are not supported. The exact release firmware comes from clean source commit `554a331`; later release commits update documentation and CI only. Settings v2 can require re-pairing when rolling back to older firmware.
