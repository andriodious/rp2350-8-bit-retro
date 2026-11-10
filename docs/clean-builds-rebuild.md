# Clean builds checkpoint — 2026-11-10

Production improvements are committed at c3c3c2b on experiment/c64-1581.
The exact combined source is tagged reference/clean-combined-38533c78b520f911.
Stable release has not changed. No remote push is implied by this checkpoint.
Uncommitted reset experiments in the development worktree are intentionally excluded.

## Saved artifacts

Relative to the repository parent directory:
- artifacts/combined-clean-c3c3c2b/presto-combined-clean.uf2 — combined reference;
  C64 38533c78b520f911, Master 92852038d2174937, Model B f1c2a7bca0c44e53.
- artifacts/c64-clean-reset-test/c64-clean-reset-test.uf2 — standalone 248af4fedbeca14f.

Their hashes are tracked in clean-builds-sha256.txt. Each artifact directory also
contains build settings, ELF, logs and its own manifest. Keep these exact files:
a rebuild may produce a new image identifier and different hashes.
The clean combined image has user-reported successful warm resets on two Prestos.
The clean standalone has no confirmed comparable hardware result yet.
The older combined USB-diagnostic restore is superseded for current clean testing.

## Rebuild configuration

Use a fresh detached worktree at the reference tag and a NEW build directory.
Do not build the dirty reset-investigation worktree or reuse its CMake cache.
Prerequisites: ARM GNU 14.2.rel1, Pico SDK 2.2.0 (including submodules), CMake,
Ninja, Python, Rust with thumbv8m.main-none-eabi, pinned TRX64 source from
third_party configuration, and verified ROM/disc assets. Existing local toolchain
and dependency paths are recorded in the archived CMake caches.

After configuring those toolchain/dependency paths, combined build:

```sh
export NINJA_JOBS=6
export OS_CMAKE_ARGS="-DSOUND=ON -DAMBIENT=ON -DNIMBUS_TEST=ON -DWEBDIAG=OFF -DC64_PROFILE=OFF -DC64_FULL_1581=ON -DC64_1581_IDLE_SRAM=ON -DC64_1581_LOADING_SRAM=ON -DC64_1581_CPU_SRAM=OFF -DC64_1581_FLUSH_LINES=16 -DC64_1581_NET_SLICES=ON -DC64_1581_DELAY_LOOP=ON -DC64_1581_JOB_SCAN=ON -DC64_1581_SERIAL_WAIT=OFF -DC64_1581_DRIVE_PROFILE=OFF -DC64_SID_FAST_STATE=ON -DC64_SID_HOT_CODE=ON -DC64_WATCHDOG=ON"
export SDK_CMAKE_ARGS="${SDK_CMAKE_ARGS:-} -DSTARTUP_DIAG=OFF -DSTARTUP_DIAG_PREMAIN=OFF"
sh scripts/build-retro.sh build-combined-clean
```

Set C64_TRX64_SOURCE in OS_CMAKE_ARGS to the verified upstream checkout when
building offline; the original used /tmp/trx64-review at
 a86341c8b2add9841dab1a8340fb76d71121e1e3.
SDK_CMAKE_ARGS can include picotool_DIR for an existing installed picotool.
Unset APP and SAVE when reproducing this reference: it includes neither extra
application payload nor save-disc writes.

For standalone, configure the top-level project with MACHINE=c64, BOARD=presto,
the same machine options and diagnostics OFF; build presto_c64. Convert its UF2
using tools/dual_boot_uf2.py --plain before flashing over a combined partition table.
Normal USB initialization and regular watchdog/fault recovery remain enabled.
C64_RAM_CPU, C64_RAM_VIC, C64_PACKED_PIXELS and C64_RENDER_OVERLAP remain ON.
This is a clean performance build, not a claim that all ordinary status/fault
reporting has been removed or that the warm-reset fault is fixed.

## Resume point

See clean-combined-reset-reference.md for the source/configuration comparison.
The next useful hardware comparison is clean standalone versus clean combined
under matching conditions. Combined physical reset normally starts Master;
standalone restarts C64. Preserve that distinction when interpreting results.
No new reset experiments or stable-release changes are required during the pause.
