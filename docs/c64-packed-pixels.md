> Historical development record. The final PR update enables the four tested optimisations by default and leaves profiling OFF. See [current defaults and validation](c64-optimised-release.md). Experimental defaults and unmeasured-result statements below describe that stage of development.

# Experiment 3: read four indexed pixels together

Parent experiment: render overlap commit 050ece3 with VIC SRAM enabled. Hardware
captures measured 44.96 emulated fps normal and 50.93 Turbo (15 windows/about 75
seconds each). Normal display wait fell from 7.21 ms to 0.002 ms, but conversion
rose from 6.37 to 13.33 ms and emulation from 16.56 to 21.97 ms. These observations
are consistent with shared-memory contention, not proof of a specific bus/cache
cause. All figures include profiling and normal web polling overhead.

## Change

C64_PACKED_PIXELS is optional and defaults OFF. The converter obtains four indexed
pixels with one aligned 32-bit read, then extracts each byte's low four bits for
the existing palette lookup. The first pixel remains duplicated for the existing
4:5 scale. No palette, row geometry, LCD driver, emulated machine or clock changes.
VIC SRAM and overlap remain enabled in the supplied experimental firmware.

The PSRAM allocator returns eight-byte-aligned allocations; each row is 384 bytes
and each group advances four bytes. memcpy into a uint32_t avoids type-aliasing
violations; the explicit alignment assumption permits one Arm LDR. The helper
requires a little-endian target, checked at compile time.

Disassembly confirms the old four input LDRB operations become one LDR per group.
This reduces executed input load instructions; it does not establish a particular
reduction in physical PSRAM bus traffic or promise a fourfold speed-up. Cache,
output stores and concurrent emulation still affect the result.

## Build

Use a new directory with all experiment options explicit:

```sh
. deps/env.sh
OS_CMAKE_ARGS='-DSOUND=ON -DAMBIENT=ON -DC64_PROFILE=ON -DC64_RAM_VIC=ON -DC64_RENDER_OVERLAP=ON -DC64_PACKED_PIXELS=ON' NINJA_JOBS=2 scripts/build-dual-boot.sh build-c64-packed
```

Output: build-c64-packed/presto_dual_boot.uf2. Normal builds still default the
experiment options OFF. No new FRANK source/checksum change is needed.

## Validation

- All 65,536 combinations of four palette indices produce exactly the scalar
  output, including inputs with arbitrary ignored high bits.
- A complete 384x272 frame matches the scalar converter, including unchanged
  output row padding and group boundary guards, under address/undefined-behaviour
  sanitizers.
- The combined experimental firmware compiles, links and packages; OFF builds
  pass separately. The OFF combined UF2 is byte-for-byte identical to the previous
  overlap image (SHA256 2f616e662be2cee0cf4b40666b338e1f17e7b352c17da71c502eb0ffa434ea42).
  Arm disassembly confirms one input word load per group.
- Static SRAM end remains 0x2006a53c (72,388 bytes before heap allocations); no
  additional framebuffer or persistent runtime allocation is introduced.
- The prior frame-overlap ownership and emulator frame-hash tests apply to the
  unchanged emulator and buffer exchange. This step changes only conversion.

Run the converter check:

```sh
c++ -std=c++20 -O2 -fsanitize=address,undefined -I src/machines/c64 tests/c64/pixel_convert_test.cpp -o /tmp/c64-pixel-test
/tmp/c64-pixel-test
```

Hardware speed, ARM access behavior and actual LCD output remain to be validated.

## Measure

Use the same Elite scene, sound/LED settings and visible web page. Answer N to
fastloading. Set Turbo OFF, wait 15 seconds, clear performance data, collect a
minute and save as c64-packed-normal.txt. Repeat with Turbo ON and save as
c64-packed-turbo.txt. Report any graphics/audio changes or instability. The
previous overlap UF2 is supplied for rollback.

Compare conversion_ms, emulation_ms and overall fps. Pacing may cap normal mode
at 50 fps if there is enough headroom; Turbo still converts alternate frames.
Do not add overlapping conversion time to emulation or wait time.
