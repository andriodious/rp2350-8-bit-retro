# Building RP2350 8-bit Retro

Use a fresh checkout/extracted source directory. Existing test builds remain
useful rollback firmware, but their CMake caches refer to their original paths.

```sh
scripts/setup.sh --no-build --roms
. deps/env.sh
scripts/build-retro.sh
```

On Debian/Raspberry Pi OS, `scripts/setup.sh --apt --no-build --roms` can install
host packages. Setup fetches the pinned Arm toolchain and Pico SDK 2.2.0, and BBC
assets. C64 configuration runs `scripts/fetch-c64.py`, which fetches the pinned
FRANK fork, applies the recorded adaptation and verifies hashes, then downloads
and verifies the configured C64 Elite disk. See `third_party/c64-sources.json`.

The output is `build-retro/rp2350-8-bit-retro-presto.uf2`, a combined launcher,
Master, Model B and C64 image. The intermediate `presto_dual_boot.uf2` is identical;
the old internal filename and partition identifiers remain for compatibility.
No separate C64 fork edit or moving-branch dependency is needed for this release.

Default release features: SOUND=ON, AMBIENT=ON, NIMBUS_TEST=ON, WEBDIAG=OFF and
C64_PROFILE=OFF. The four C64 optimisations remain enabled by default. NIMBUS_TEST
is a legacy build-option name; it enables the complete original Nimbus support.
For example, to include web diagnostics in a separate build:

```sh
OS_CMAKE_ARGS="-DSOUND=ON -DAMBIENT=ON -DNIMBUS_TEST=ON -DWEBDIAG=ON -DC64_PROFILE=OFF" \
  scripts/build-retro.sh build-retro-diag
```

For firmware without Bluetooth, pass NIMBUS_TEST=OFF in a new build directory.
Pass NINJA_JOBS=2 on a memory-constrained host. `SDK_CMAKE_ARGS` remains available
for platform-specific SDK tools. Existing `scripts/build-dual-boot.sh` and
single-machine CMake targets remain valid. The inherited setup command without
`--no-build` still uses the legacy combined-build output path; the commands above
are the recommended branded entry point.

The clean firmware has sound controls but hides test traces and raw controller
readouts. Controller pairing, maps and reconnect remain visible. Existing saved
pairing, Wi-Fi and disk-save locations are unchanged. Clock frequency is 250 MHz.
