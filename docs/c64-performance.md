> Historical development record. The final PR update enables the four tested optimisations by default and leaves profiling OFF. See [current defaults and validation](c64-optimised-release.md). Experimental defaults and unmeasured-result statements below describe that stage of development.

# C64 performance work: measurement baseline

Starting PR #10 commit: `a9360b0aa99d8f28ab7eba75973e1938688c19d2`.
This first change measures the existing implementation. It does not change the
250 MHz clock, allocation policy, rendering algorithm, or emulated timing.
No speed improvement or hardware result is claimed yet.

## Browser collection (no USB logging required)

With C64_PROFILE=ON, the usual web interface shows a **C64 performance** panel
when C64 is selected. WEBDIAG is not required. The existing status poll reads the
last completed window without resetting counters; repeated windows are ignored.

Leave Elite on Load New Commander, wait 15 seconds, then click **Clear performance
data**. Keep this page visible for one minute and click **Save performance data**.
Repeat after enabling Turbo and waiting 15 seconds. Rename the downloads to
c64-normal.txt and c64-turbo.txt. Keep sound and LED settings unchanged. Browser
polling affects timings, so use the same open-page conditions for both runs.

The page retains up to 120 samples in memory (roughly ten minutes). Save before
refreshing or leaving the page. No extra polling loop or firmware log allocation
is added. Downloads use the same C64PROF text format as USB captures. A stalled
network can lose intermediate windows; the frame/time values describe each saved
window, not the browser's collection duration.

## Build and collect

Configure a fresh combined build:

```sh
. deps/env.sh
OS_CMAKE_ARGS='-DSOUND=ON -DAMBIENT=ON -DC64_PROFILE=ON' NINJA_JOBS=2 scripts/build-dual-boot.sh build-c64-profile
```

C64_PROFILE defaults OFF. Diagnostics in the web UI remain separately controlled
by WEBDIAG; it is not required for these measurements.

Flash `build-c64-profile/presto_dual_boot.uf2`. Select C64, answer N to Elite's
fastloader prompt, and leave Elite on the Load New Commander screen. Keep sound
and LEDs at the same settings between runs. Let the scene settle for 15 seconds.
With Turbo OFF, capture:

```sh
python3 tools/c64_profile.py --seconds 60 --output c64-normal.txt
```

Enable Turbo, wait 15 seconds and capture:

```sh
python3 tools/c64_profile.py --seconds 60 --output c64-turbo.txt
```

Use `--port /dev/ttyACM0` if automatic detection fails. Install `python3-serial`
with apt if needed. The capture tool refuses to overwrite existing reports.
It passively reads USB; close other serial monitors. The firmware emits one
C64PROF line per approximately five seconds. `python3 tools/presto.py c64prof`
also requests the last completed window (initially all zeros).

## Interpretation

All `_us` values are cumulative microseconds in the reported window. Divide by
`frames` for average time per emulated frame. Divide conversion_us by `rendered`
for time per converted frame. Frames per second = frames * 1000000 / window_us.
PAL real time is 50 frames per second. Turbo currently skips alternate conversions.

- cpu_us, vic_us, sid_us and cia_us cover the line-level core calls. CPU includes
  disk traps and other work invoked from the interpreter. CIA is the separate
  line-level timer calls; any work inside the CPU is counted under CPU instead.
- emulation_us includes those calls, input and other frame work. Do not add it to
  the individual chip times. Sub-microsecond timing and instrumentation overhead
  mean chip totals are approximate.
- conversion_us is core 1's pixel conversion, collected after the existing frame
  hand-off. wait_us is core 0 waiting for that hand-off, including frontend polling
  and any remaining core-1 LED work. They overlap: do not add them together.
- frontend_us is the explicit frontend_frame call. Additional frontend polling is
  included in wait_us and pacing_us.
- pacing_us includes the intentional normal-speed wait and its frontend polling.
- Other housekeeping is not individually timed. Values are not a complete CPU
  utilisation accounting, and profiler overhead can reduce measured speed.

Counters are written only on core 0; conversion time is read after the renderer's
existing barrier. A change in Turbo starts a fresh window at the next frame;
installation pauses discard the current window. Discard transitional scene windows.
There is no instruction sampling or heap-allocation tracking.

## Checks and next decisions

The combined profile build and the C64 build with profiling OFF compile/link.
Host checks cover timing-window accounting, mode reset, timer wrap and OFF call
semantics. Native core checks with profiling disabled pass 1,000 BASIC frames and
10,000 Elite frames under address/undefined-behaviour sanitizers. Dependency patch
and checksum checks pass from the pinned fork archive. Hardware measurements are
pending; host timings are not a substitute for RP2350 measurements.

After normal/Turbo captures, choose the next experiment from selective hot-code
SRAM placement, hot-object SRAM placement, or render/emulation overlap. Keep each
experiment in a separate commit and compare the same scene and settings. Confirm
any gain with profiling OFF before claiming a release improvement.

FRANK's pinned revision is unchanged. The BBC adaptation patch and expected hash
for C64_rp2350.cpp include the optional timing hooks. Existing cached dependencies
from r11 must be moved aside before fetching the revised adaptation; do not discard
local edits. No change to the shared FRANK fork is required.

Web follow-up checks: profile JSON bounds and simulated-browser panel visibility, deduplication, clear and download. Full combined firmware rebuilt with profiling ON.
