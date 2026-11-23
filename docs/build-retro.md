# Building RP2350 8-bit Retro

Use a fresh checkout/extracted source directory. Existing test builds remain
useful rollback firmware, but their CMake caches refer to their original paths.

```sh
scripts/setup.sh --no-build --roms
. deps/env.sh
EXTRA_OS_CMAKE_ARGS='-DC64_ABORT_DIAG=ON -DC64_RUNTIME_DIAG=ON' scripts/build-classic-keyboard-four.sh build-release
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
C64_PROFILE=OFF. The release command also sets C64_ABORT_DIAG=ON and
C64_RUNTIME_DIAG=ON to retain terminal-caller capture and the read-only
`/api/c64-runtime` register/event observer used by the hardware-tested firmware.
Both underlying CMake defaults remain OFF; the runtime endpoint is independent
of WEBDIAG. The four C64 optimisations remain enabled by default. NIMBUS_TEST
is a legacy build-option name; it enables the complete original Nimbus support.
For an independent build with web diagnostics, set `EXTRA_OS_CMAKE_ARGS="-DC64_ABORT_DIAG=ON -DC64_RUNTIME_DIAG=ON -DWEBDIAG=ON"` when calling the keyboard build script. For a custom build without Bluetooth, use the lower-level CMake options with NIMBUS_TEST=OFF, BT_KEYBOARD_CLASSIC=OFF and BT_KEYBOARD_DISCOVERY=OFF; the release wrapper deliberately enables Bluetooth.

Pass NINJA_JOBS=2 on a memory-constrained host. `SDK_CMAKE_ARGS` remains available
for platform-specific SDK tools. Existing `scripts/build-dual-boot.sh` and
single-machine CMake targets remain valid. The inherited setup command without
`--no-build` still uses the legacy combined-build output path; the commands above
are the recommended branded entry point.

The release firmware has sound controls and hides general web test traces and raw
controller readouts. Its C64 runtime observer remains available through the web
API. Controller pairing, maps and reconnect remain visible. Existing saved
pairing, Wi-Fi and disk-save locations are unchanged. Clock frequency is 250 MHz.

## v0.9.2 four-machine release

The release retains the exact hardware-accepted combined UF2 from source commit
`863096bc0492ca600682f1765c8f12cb7d2afb60`. The release commit adds documentation without rebuilding it.
The source checkpoint is `reference/four-machine-wifi-gate-20261123`.

Keep APP and SAVE unset. Use a fresh output directory for the recipe above;
existing build directories retain their cached CMake options. C64 terminal-caller
capture, runtime diagnostics and D81 performance options remain enabled.
Spectrum's fork is pinned in `src/cores/chips/UPSTREAM`; Elite is downloaded and
verified by `scripts/fetch-spectrum.py`. Commercial ROMs and game images are not
added to Git by this release.

Firmware SHA256: `3a514d70d51d1e0ac79ae6c7966b0a2904364f13a3318b8517ba386dd5ea3889`.
See the [verification record](../checkpoints/four-machine-wifi-gate-20261123/verification.json).
Source rebuild image IDs and labels can differ. Rebuilds and CI artifacts are not
automatically hardware verified. v0.9.1 and all earlier checkpoints remain available.
