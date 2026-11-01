> Historical development record. The final PR update enables the four tested optimisations by default and leaves profiling OFF. See [current defaults and validation](c64-optimised-release.md). Experimental defaults and unmeasured-result statements below describe that stage of development.

# Experiment 4: main 6510 interpreter in SRAM

Parent: packed-read experiment 1ea26e9. User measurements of that build were
45.68 emulated fps normal and 51.59 Turbo over 15 windows each. Normal CPU time
was 8.90 ms/frame and overall emulation 21.62 ms/frame; display waiting was
negligible. Figures include the profiler and the same visible browser page.

C64_RAM_CPU is optional and defaults OFF. It places only MOS6510::EmulateLine in
`.time_critical.c64_cpu`, which the existing SDK startup copies to SRAM. Other
CPU routines and constants retain their existing placement. No interpreter
algorithm, clock, renderer, audio or pacing change is made. The supplied image
keeps VIC SRAM, overlap, packed reads and profiling enabled at 250 MHz.

This adds a conditional section attribute to the pinned FRANK adaptation patch;
the expected CPUC64.cpp checksum is updated. The source revision and shared fork
remain unchanged. Existing cached adapted source must be preserved/moved aside
before re-fetching; the fetch script refuses to overwrite it automatically.

## Memory and validation

The interpreter routine is 23,976 bytes. Including link alignment/veneers the
static SRAM cost is 24,576 bytes: end moves from 0x2006a53c to 0x2007053c.
Space to the ordinary heap limit is 47,812 bytes, down from 72,388 bytes. This is
not free memory after Wi-Fi/runtime allocations. No extra PSRAM allocation is made.

ON/OFF combined builds pass. The OFF combined image is byte-for-byte identical
to the previous packed-read firmware (SHA256
a392018da4099ffcdfaf163bf0a60a03d98c6a5ae59b9e2a7f55c5489cdba21f). The placement test checks the CPU and VIC addresses
and uses an explicit 46 KiB minimum link-time headroom guard for CPU-on builds
(64 KiB otherwise). This threshold only catches unintended growth; it is not a
proof of runtime capacity. Dependency extraction/application/checksum/reuse/edit
protection tests pass with the supplied pinned-fork archive.

The interpreter source changes only by an optional placement attribute. Previous
emulator/frame-equality checks cover the unchanged algorithm; no new hardware
stability or speed result is claimed. Real ARM relocation and the smaller live
heap require testing on Presto. Do not enable by default based on build success.

```sh
python3 tests/c64/ram_placement_test.py /path/arm-none-eabi-nm /path/presto_c64.elf on cpu-on
```

## Build and measure

From the prepared source in a fresh output folder:

```sh
. deps/env.sh
python3 scripts/fetch-c64.py
OS_CMAKE_ARGS='-DSOUND=ON -DAMBIENT=ON -DC64_PROFILE=ON -DC64_RAM_VIC=ON -DC64_RENDER_OVERLAP=ON -DC64_PACKED_PIXELS=ON -DC64_RAM_CPU=ON' NINJA_JOBS=2 scripts/build-dual-boot.sh build-c64-cpu-sram
```

Output: build-c64-cpu-sram/presto_dual_boot.uf2.

Repeat the same Elite scene/sound/LED/browser settings. Answer N to fastloading.
With Turbo OFF wait 15 seconds, clear data, collect one minute and save as
c64-cpu-sram-normal.txt. Repeat with Turbo ON as c64-cpu-sram-turbo.txt.
Also report the RAM used/total shown by the web interface while Elite is running,
and confirm the SD list and disk reload still work. Note freezes, resets, graphics
or sound changes. The previous packed-read image is supplied for rollback.

Compare overall fps and CPU time, while also inspecting VIC/SID/conversion times
for changes in shared flash/cache contention. Improvements remain a hypothesis.
