> Historical development record. The final PR update enables the four tested optimisations by default and leaves profiling OFF. See [current defaults and validation](c64-optimised-release.md). Experimental defaults and unmeasured-result statements below describe that stage of development.

# Experiment 1: VIC code in internal SRAM

Baseline: web profiling commit a0f84c2, based on PR #10 a9360b0.
The user's 14 normal windows (70.16 seconds) measured 36.90 emulated frames/sec;
17 Turbo windows (85.21 seconds) measured 41.80 frames/sec. Normal-mode VIC cost
was 8.46 ms/frame, CPU 6.67 ms/frame, SID 3.70 ms/frame, emulation total 19.53
ms/frame, and display wait 7.49 ms/frame. These are profiled/web-polled results,
not release-firmware benchmarks. Raw input files remain with the task artifacts.

## One change

`C64_RAM_VIC=ON` excludes VIC.cpp's text sections from the flash text output.
The SDK's normal SRAM data/startup-copy mechanism collects them instead, using
an approach already used by the Master. Constants remain in flash. The filename
uses CMake's platform-specific C++ object extension. Unexpected SDK linker scripts
fail configuration rather than silently ignoring the option.

Default is OFF. The CPU, SID, allocation policy, rendering algorithm, frame pacing,
Turbo behavior, clock (250 MHz) and web profiler are unchanged. FRANK source and
its checksum/adaptation patch are unchanged by this experiment.

Graphics code alone is the first experiment because it is the largest measured
emulation category and costs less SRAM than moving CPU and graphics together.
This does not yet implement rendering/emulation overlap.

## Build

From a checkout containing the cumulative profiling changes and this experiment:

```sh
. deps/env.sh
OS_CMAKE_ARGS='-DSOUND=ON -DAMBIENT=ON -DC64_PROFILE=ON -DC64_RAM_VIC=ON' NINJA_JOBS=2 scripts/build-dual-boot.sh build-c64-vic-sram
```

Use a new build folder because the combined script reuses existing configurations.
Output: `build-c64-vic-sram/presto_dual_boot.uf2`.
For a baseline comparison, use another fresh folder with C64_RAM_VIC=OFF.

## Verification

Both variants compile/link and package successfully with the same toolchain/SDK
as the profiling baseline. The OFF combined UF2 is byte-for-byte identical to
the previously delivered web profiling UF2 (SHA256
5110e6fc63ce5421edacafedb53ac1ffec696827b61224ddaea51f212dc6bc1e).

The ELF check confirms all linked MOS6569 methods execute in SRAM with ON and
flash with OFF. MOS6510::EmulateLine remains in flash. SRAM static end changes
from 0x2006892c to 0x2006a52c: 7,168 extra bytes. Space up to the normal heap limit
is 79,572 bytes OFF and 72,404 bytes ON. This is capacity before runtime
allocations, not measured free memory with Wi-Fi active.

```sh
python3 tests/c64/ram_placement_test.py /path/arm-none-eabi-nm /path/presto_c64.elf on
```

The test rejects unexpected placement and less than 64 KiB of link-time heap
headroom; that threshold is a regression check, not a guarantee of runtime capacity.
No emulator/video algorithm was modified, so the earlier native core checks still
apply to the same source. Host execution cannot validate ARM relocation or real
PSRAM/cache behavior. Actual panel output, Wi-Fi stability and speed must be checked
on Presto before enabling this in a normal release. No speed gain is claimed yet.

## Measure

Flash the experimental combined image. Verify Master boot and C64 switching, then
use the same Elite screen, sound/LED settings and visible browser page as before.
Answer N to fastloading. Wait 15 seconds, clear performance data, collect one minute
and save with Turbo OFF; repeat with Turbo ON after a 15-second settling period.
Send `c64-vic-sram-normal.txt` and `c64-vic-sram-turbo.txt` plus any visual/audio or
web responsiveness changes. If behavior regresses, flash the supplied baseline.
