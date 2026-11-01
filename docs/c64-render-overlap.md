> Historical development record. The final PR update enables the four tested optimisations by default and leaves profiling OFF. See [current defaults and validation](c64-optimised-release.md). Experimental defaults and unmeasured-result statements below describe that stage of development.

# Experiment 2: overlap emulation and display conversion

Starting point: VIC SRAM experiment 67dd394, with web profiling. User measurements
for that experiment were 41.92 emulated frames/sec normal and 48.03 Turbo, versus
36.90 and 41.80 in the original profiling baseline. The remaining normal-mode
wait was 7.21 ms/frame; emulation itself was 16.56 ms/frame.

## Design

C64_RENDER_OVERLAP is optional and defaults OFF. With it ON, Display allocates a
second 384 x 272 indexed framebuffer in PSRAM (104,448 bytes). Core 0 writes the
current frame while core 1 converts the previous completed frame into the existing
RGB565 LCD buffer. The LCD framebuffer and PIO driver are unchanged.

The existing c64_run_frame returns at raster 0. At that boundary, after the previous
conversion has completed, the completed indexed buffer is published and the spare
becomes writable. The VIC reacquires BitmapBase at raster 1 before visible lines.
There is no per-frame framebuffer copy. If core 1 is late, core 0 waits before the
exchange, never overwriting a buffer still being read. On Turbo-skipped frames,
the current writable buffer is reused without an exchange.

The frame-ready flag is a lock-free C++ atomic. Release/acquire publication protects
the source pointer/pixels and completion protects reuse and conversion statistics.
Only core 0 changes Display pointers. A pending job uses a captured pointer rather
than asking Display for the now-writable buffer. Reset continues on the writable
buffer; installation pauses can leave the reader finishing its existing job.

The existing source licenses, FRANK fork/revision, adaptation patch/checksums,
250 MHz clock, CPU/VIC/SID emulation and LCD conversion algorithm are unchanged.
No upstream FRANK source change is needed for this experiment. VIC SRAM stays ON
in the supplied test firmware. No speed improvement is claimed before measurements.

## Build

Use a fresh build directory in the prepared source tree:

```sh
. deps/env.sh
OS_CMAKE_ARGS='-DSOUND=ON -DAMBIENT=ON -DC64_PROFILE=ON -DC64_RAM_VIC=ON -DC64_RENDER_OVERLAP=ON' NINJA_JOBS=2 scripts/build-dual-boot.sh build-c64-overlap
```

Output: `build-c64-overlap/presto_dual_boot.uf2`.
The profiling, VIC SRAM and overlap options remain independently selectable.

## Validation

- Combined profiling+VIC SRAM+overlap firmware compiles/links/packages.
- C64 build with profiling and overlap OFF compiles/links.
- All 11,000 per-frame indexed framebuffer hashes match overlap OFF/ON in the
  native sanitizer harness (1,000 BASIC and 10,000 frames after loading the Elite
  disk). The test alternates blocks of every-frame and alternate-frame exchanges.
- The same test retains the previous reader pointer across the next emulated
  frame and verifies that its contents are unchanged; exchanges never return the
  new writable pointer to the reader. It includes the harness's C64 reset.
- Frame sequence hash: 67573647525656ced4f92f0b970192359855e06f87be01d9aa9e50d13f6c4534.
- Linked VIC routines remain in SRAM. Static SRAM ends at 0x2006a53c; space to the
  ordinary heap limit is 72,388 bytes before runtime allocations (16 fewer than
  VIC-only). Extra PSRAM framebuffer allocation is 104,448 bytes plus allocator
  overhead. Allocation failure explicitly panics.

Run the frame comparison with `python3 tests/c64/frame_overlap_test.py` after
fetching the dependencies. It uses the existing native build prerequisites and
address/undefined-behaviour sanitizers; leak detection follows the native harness
and is disabled. This checks pixel equality and ownership under a sequential host
schedule, not physical dual-core timing, LCD scanout or PSRAM bandwidth.

## Hardware comparison

Repeat the same Elite screen and visible browser page with unchanged sound/LED
settings. Wait 15 seconds, clear the performance panel, capture a minute with Turbo
OFF and save; repeat with Turbo ON. Name the downloads `c64-overlap-normal.txt`
and `c64-overlap-turbo.txt`. Report visual corruption, audio changes, resets or web
responsiveness problems. Keep the VIC-only firmware for rollback.

The expected opportunity is lower wait_us. Emulation or conversion time may rise
because both cores now access PSRAM concurrently, so assess overall fps as well.
Normal pacing can cap emulation at 50 fps; Turbo reveals additional headroom while
still converting alternate frames. Conversion statistics now count completed jobs
reaped at the next frame boundary, so a five-second window can differ by one job.
Do not add conversion time to wait time or emulation time: these now overlap.
The RGB565 scanout buffer is still single-buffered, as before; this change does not
promise tear-free output. Hardware validation remains required before release use.
