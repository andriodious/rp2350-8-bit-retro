# Experimental full 1581 drive

This optional build runs a separate 6502 CPU, 8520 CIA and WD controller for
819200-byte D81 images. It supports uploaded drive programs such as the M-W/U3
loader used by Monkey Island v1.2. The normal build remains unchanged.

The prototype derives its drive implementation from Jondalar/TRX64, pinned at
`a86341c8b2add9841dab1a8340fb76d71121e1e3`, including its VICE-derived components.
Generated source retains upstream notices. See the root GPLv3 LICENSE and
[TRX64](https://github.com/Jondalar/TRX64). Commodore ROM rights remain with their
respective owners; the ROM is downloaded separately, checked by SHA-256 and
kept in the ignored ROM cache.

## Build

Prepare the usual Pico SDK and Arm tools, plus Rust/Cargo and its soft-float ABI
ARM target (the Pico SDK uses this calling convention):

```sh
rustup target add thumbv8m.main-none-eabi
OS_CMAKE_ARGS='-DSOUND=ON -DAMBIENT=ON -DNIMBUS_TEST=ON -DWEBDIAG=ON -DC64_PROFILE=ON -DC64_FULL_1581=ON -DC64_1581_IDLE_SRAM=ON -DC64_1581_CPU_SRAM=OFF -DC64_1581_NET_SLICES=ON' \
  scripts/build-retro.sh build-1581-test
```

Use a fresh build directory when changing these options. The build fetches the
pinned TRX64 checkout into `deps/trx64-1581`, prepares a separate patched Frodo
copy in the build directory and downloads the checksum-verified 1581 ROM.
`-DC64_TRX64_SOURCE=/path/to/pinned/TRX64` selects an existing checkout. An
incorrect revision or tracked modifications are rejected. Rust was tested with
1.99.1. No external Cargo packages are needed for the extracted drive crate.

Output: `build-1581-test/rp2350-8-bit-retro-presto.uf2`.

## Hardware test

1. Flash the combined UF2 using the usual BOOT/RESET procedure.
2. Open `http://8bitretro.local`, select Commodore 64 and load the Monkey Island
   D81 through the existing SD-card or upload controls.
3. Allow the original KERNAL RAM test to finish. Autoload starts after 250
   emulated frames; the full drive then performs the game's loader sequence.
4. Check whether it reaches the mountain/title graphic and opening story.
5. Check sound, web responsiveness, Nimbus input, and switching back to Master.

`/api/status` includes `drive1581.active`, `drive1581.read_only` and the drive
CPU `pc`. The existing diagnostics and performance samples are enabled in the
test build. If loading stalls, retain those samples and the last visible screen.

D81 media is **read-only** in this prototype: game saves are not supported.
Other formats retain their existing DOS-level, tape/program or cartridge paths.
Ejecting a disk destroys the full drive; C64 reset resets its electronics while
keeping the mounted disk. Selecting another D81 creates a fresh drive.

## Validation and limits

The firmware's C ABI and generated Frodo source pass the native M-W/U3 execution
probe, repeated mount/reset/eject checks, and a 10,000-frame Monkey Island run.
The inspected frame shows the mountain title and opening story. C/C++ uses ASan
and UBSan; Rust is an optimized build without sanitizer instrumentation.

The three-machine firmware links and the UF2 packer accepts all partitions.
C64 occupies approximately 1092 KiB of its 4 MiB flash slot. The linker leaves
41,976 bytes between static SRAM data and the heap limit, before runtime heap
use. The core-0 stack remains 16 KiB; the largest individual new ARM stack frame
observed was 2624 bytes. This is not a measured worst-case call-chain watermark.
Large drive allocations use PSRAM, and the original unbounded diagnostic event
list is disabled.

Desktop success does not establish on-device speed, audio continuity, long-run
stability, or full-game compatibility. This is a hardware-test build, not a
published release. Its `DIRTY` suffix correctly identifies uncommitted work.

## Performance test 2

The first hardware run loaded and ran Monkey Island, but measured approximately
17–24 frames/s with core 0 at 98–99% and slow web responses. This establishes a
performance problem; it does not identify the share attributable to each stage.

The second test build removes the C-to-Rust call and RefCell access from every
C64 instruction. A core-0-only inline counter accumulates the same cycles, consumed
at the existing scanline and CIA2 serial synchronization points. The 1581's 8 KiB
RAM allocation uses a dedicated SRAM buffer; disk, ROM and other allocations
remain in PSRAM. Static heap headroom is now 33,776 bytes. Clock speed remains
250 MHz and no drive cycles are skipped.

The upload/U3 probe and repeated mount/reset/eject allocation checks pass. All
20 captured frames of the 10,000-frame Monkey Island intro match the first
firmware harness byte-for-byte. The built ARM image has no calls to the old
per-instruction `retro_drive_tick` function. The web trace now retains
`drive1581`, `c64_profile`, `speed` and `mem` fields. On-device performance gains
remain to be measured; this build does not claim a restored 50 frames/s.

## Web responsiveness measurements

The diagnostics-enabled web page records browser-observed API completion time,
including the response body. It shows the last status request time and the
median, 95th percentile and maximum among the last 100 successful status
requests. Error and timeout totals count from page load or Clear trace; failed
requests are excluded from the successful-response percentiles. Aborted response
bodies count as timeouts too.

Each STATUS trace includes `web` with separate `status` and `controls` totals,
latency summaries and the latest result. Control requests and failed requests
also produce `WEB_REQUEST` lines identifying the API path and method, elapsed
milliseconds, outcome and HTTP status. Request bodies are not recorded.
Browser visibility is included because background tabs can distort timings.
This measures browser/network/server latency together, not firmware CPU time.
No extra network requests are generated.

After flashing the updated test firmware, refresh the page and keep the tab
visible. Clear the trace before each D81 versus D64/CRT comparison, exercise a
few controls, then save the trace. A page reload resets the timing counters,
while previously saved trace lines retain their original timing data.

## Dedicated drive timing build

`C64PROF` and `c64_profile` now include `drive_us`: elapsed time inside the 1581
core's `run_cycles`, accumulated across both serial-access catch-up and scanline
catch-up within the profiling window. This includes its CPU/CIA/disk-controller
execution, but excludes surrounding bridge bus bookkeeping. It overlaps
`emulation_us` and may overlap `cpu_us`; never sum it with those totals.
`drive_us / frames / 1000` gives drive milliseconds per emulated frame, while
`100 * drive_us / window_us` gives its share of elapsed wall time.

`drive1581.run_us` and `run_calls` expose cumulative counters since drive
mount/reset. When comparing D81 with CRT, discard the window spanning the media
change. The timer adds instrumentation overhead; use it to locate the cost, not
as a promise of improved speed. The web responsiveness instrumentation remains
included. Host tests cover profiling windows, disabled-drive output, drive-counter
reset, and unchanged captured Monkey Island frames (20 of 20).

## ROM placement comparison

The next experimental build borrows the permanent embedded 1581 ROM directly
from flash instead of retaining a 32 KiB PSRAM copy. Drive RAM remains in SRAM,
and disk data remains in PSRAM. The C ABI ROM pointer must remain valid and
immutable for the drive lifetime; the firmware passes its embedded const array.
The owned-ROM setter remains available to the other portable harnesses.

The U3 probe, repeated lifecycle checks and all 20 intro frame captures pass
unchanged. Drive timing instrumentation remains enabled. This change tests ROM
access cost only: flash-cache contention could affect the result, so improvement
is not assumed until hardware samples are compared.

## Single-drive bus bookkeeping comparison

The next build configures the one-drive mapping only on mount/reset. Catch-up
no longer rechecks that mapping or copies the IEC contribution of all absent
devices. A fixed released-bus array is valid because this firmware exposes only
one drive at unit 8; the drive core ignores its own entry and retains its own
output. Actual C64 bus inputs still come from the live IEC state. This shortcut
must be removed or generalized before supporting a second serial device.

No cycles or serial synchronization points are skipped. Native U3 and lifecycle
checks pass, and all 20 Monkey Island frame captures match the flash-ROM baseline.
The comparison retains that baseline's ROM placement and timing instrumentation.
Hardware improvement is unmeasured. Bridge bookkeeping is outside `drive_us`,
so compare frame rate and total emulation time as well as drive execution time.

## Instruction/work counters

The next diagnostic build adds cumulative `drive1581.instructions`,
`empty_calls`, and `empty_us`. Instructions are counted at opcode fetch;
`empty_calls` counts timed `run_cycles` invocations that fetched no opcode,
and `empty_us` is their subset of `run_us`. Requests with no advance in the C64
clock do not invoke `run_cycles` and are not included in `run_calls`. Counters
reset on drive reset/mount, and read zero after eject. Counter increments add
measurement overhead, so this is not itself an optimization.

The host C ABI counter test covers nonempty/empty work, timer wrap, reset and
eject. U3 and all 20 captured game frames pass unchanged. Compare two raw status
responses from the same mounted D81 session, about 10 seconds apart, without
resetting or changing media between them.

## Inline RAM/ROM bus experiment

Hardware work-counter samples spanning 30.497 seconds produced 531 frames
(17.41 fps). Drive execution used 65.57% of wall time. Only 250 of 1,191,121
calls fetched no instruction, consuming 1,006 microseconds: 0.005% of drive
execution time. Empty-call removal is therefore not the next optimization.

The generated 1581 bus now forces the small RAM, ROM and open-bus paths inline,
while retaining the original mapped read/write functions as non-inline helpers
for CIA and WD accesses. Open-bus last-data updates, access ordering, clocks,
interrupts and serial synchronization are unchanged. No instruction is skipped.
The pinned upstream checkout remains unmodified.

ARM symbols confirm the separate mapped helpers. The interpreter grows from
7,072 to 9,036 bytes per specialization, trading flash code size for fewer calls.
This could increase cache pressure; only hardware can establish a speed gain.
The linked heap region remains 33,752 bytes before runtime allocations. Clock
remains 250 MHz. No new RAM buffer is added.

Validation: native U3 probe, repeated mount/reset/eject checks, ASan/UBSan C++
harness, counter reset/wrap checks and profile tests pass. All 20 Monkey Island
frame captures match the previous work-counter baseline byte for byte. Rust
code itself is not sanitizer-instrumented. No hardware result is claimed yet.

Test the D81 with sound and turbo off, wait until the same game screen, then
capture two complete /api/status responses about 15 seconds apart. Keep the
Nimbus awake or otherwise hold its state consistent. After switching to CRT,
wait at least 10 seconds before taking the control sample so the five-second
profiling window no longer contains D81 work. Web trace still includes request
latency metrics; raw status alone does not measure browser response latency.

## Inline-bus hardware result and SRAM dispatcher trial

Hardware confirmed approximately 28.2 fps across 51.4 seconds with Monkey Island
D81, compared with the earlier 17.4 fps run. The scenes and controller state can
vary, so the roughly 62% gain is a workload observation, not a universal speedup.
CRT returned to 50 fps, core 0 around 60%, all drive counters zero, and Nimbus
connected. Free RAM fluctuated between 29,168 and 30,912 bytes without an observed
monotonic decline. Retain the inline-bus optimization.

The next independent experiment enables C64_1581_CPU_SRAM (default OFF). A copy
of the compiled Rust archive relocates only the built-in controller's instruction
dispatcher into Pico's .time_critical section, copied to SRAM at startup. The
unused host-controller specialization remains in flash. Cargo's original archive
is retained. The script requires exactly one matching section and a maximum
10 KiB dispatcher; compilation changes that violate these assumptions fail.
This changes code placement only, preserving interpreter semantics and clocks.
The SRAM allocation is static even when a cartridge is active. Inactive drive
execution remains zero; this experiment does not claim zero RAM overhead.

SRAM build verification: the 9,036-byte dispatcher is at 0x20007748; the unused
host dispatcher stays in flash. Linked heap space is 24,536 bytes (previously
33,752). Alignment and linker veneers make the total SRAM cost 9,216 bytes.
Archive instruction bytes match the flash build, and all relocation offsets,
types and targets are preserved apart from the intended section name change.
Firmware packing/bounds checks pass. Native game tests from the inline-bus build
still cover the unchanged interpreter logic; SRAM execution needs hardware testing.

For an existing build directory, explicitly reconfigure CMake with
-DC64_1581_CPU_SRAM=ON: the build wrapper only applies OS_CMAKE_ARGS when first
creating a build directory. Use OFF to restore the flash dispatcher. Do not
assume that changing the environment alone updates a cached build option.

## Retained flash dispatcher and cross-crate compiler experiment

The SRAM trial produced about 24.6 fps over 14.1 seconds and about 21.3 ms of
measured drive work per frame, with 19,952 bytes free in the hardware samples.
Its drive PC was in a different routine from the previous flash samples, so
these data do not isolate a regression. The user elected to keep the tested
inline-bus/flash configuration because no benefit justified the extra 9 KiB.
C64_1581_CPU_SRAM is explicitly OFF in the next build.

The next test adds lto="thin" to the firmware Rust release profile, retaining
opt-level=2 and codegen-units=1. This allows optimization across the adapter/core
crate boundary without source changes to CPU, CIA, WD or bus timing. It does
not apply LTO to the C++ emulator or BBC machines. This is a compiler experiment;
no hardware speed gain is assumed. Generated Cargo profiles must be rebuilt
when switching versions. Remove the lto setting to reproduce the inline-bus
compiler baseline.

Both drive interpreter specializations are verified in flash in the linked ELF.
Link-time heap capacity is restored to 33,752 bytes; the C64 slot rounds to
1,100 KiB versus 1,105 KiB previously. U3 and repeated lifecycle checks pass in
the native C++ ASan/UBSan harness (Rust itself is not sanitizer-instrumented).
All 20 game captures match the inline-bus baseline byte for byte. Counter
reset/wrap tests, profiling checks and UF2 placement/bounds tests pass.

Compare two D81 samples at the same game screen, 15 seconds apart, followed by
a CRT control after at least 10 seconds. Sound/turbo remain off, clock 250 MHz.
Use the same game phase: a single drive-PC snapshot does not by itself identify
the complete workload. Web response latency still requires the browser trace;
frontend_us is not a measurement of HTTP round-trip latency.

## Selected baseline after hardware trials

Retain inline RAM/ROM bus accesses, dispatcher in flash, opt-level=2 and
codegen-units=1 without explicit cross-crate LTO. The user accepted reverting
the ThinLTO experiment. Its latest window reached 24.4 fps, with 21.9 fps
across two samples and about 25 ms of drive work per frame in the latest
window. Workload phases differed, so these observations do not establish a
controlled regression; they demonstrate no reason to replace the measured
inline-bus baseline. Free RAM remained 30,912 bytes and Nimbus connected.

The SRAM experiment remains available only behind its default-OFF option;
C64_1581_CPU_SRAM is explicitly OFF for the restored build. Full 1581 support
and diagnostics remain experimental. No source changes have been published.

## Uploaded $06E7 loop and network servicing

The native run of the supplied Monkey Island D81 captures all 8 KiB of drive
RAM alongside each game frame in the external test output directory. The last
18 of 20 captures contain this sequence:

    $06E1  CLI
    $06E2  LDA #$01
    $06E4  BIT $4001
    $06E7  BEQ $06E4
    $06E9  SEI

Thus the observed $06E7 is the branch of a polling loop, testing bit 0 of
1581 CIA port B. The pinned board implementation maps that input to serial
DATA. This is reproduced uploaded code, not a direct dump from the user's
physical Presto. IRQs are enabled during the wait. The CIA read also maintains
last-read state and may process pending alarms. A future shortcut must check
the actual code bytes (RAM can change), accumulator and relevant flags, observe
pending interrupts and timer deadlines, preserve instruction-boundary timing,
and stop at serial input changes. No shortcut was enabled in that earlier network-polling experiment; see the current candidate below.
A repeated PC alone is not a sufficient shortcut guard.

Full-drive builds now call a packet-only service hook after every eight completed
C64 raster lines, only while retro_drive_enabled is true. This is outside the
Rust borrow and after the CPU/drive work for that line. net_poll retains its
500 microsecond throttle. It receives/sends transport data; net_frame still
executes API actions and installs pending media between frames. We deliberately
avoid frontend_poll here because its USB handling can execute commands during
an emulated frame. No BBC execution path is changed.

/api/status adds c64_net: calls, poll_us, max_gap_us. These are cumulative hook
calls, elapsed hook polling time, and largest interval between hook starts while
the drive remains active. They reset after a frame sees the drive inactive;
max_gap_us includes frame-boundary work and is not an HTTP latency measurement.
The outer emulation timer includes these polls, so it now includes network
service work; drive_us remains separately timed. The browser trace includes
c64_net and its existing end-to-end status/control latency metrics.

Native tests assert service hooks run with the drive active, can query its state
without a nested Rust borrow, and stop after eject. U3 and repeated lifecycle
checks pass; all 20 captured game frames match the inline-bus baseline. C++
ASan/UBSan are enabled (Rust is not sanitizer-instrumented). The ARM build,
UF2 bounds test and web JavaScript syntax checks pass. No hardware responsiveness
improvement is claimed until the new trace is measured.

Hardware procedure: reload the page after flashing, clear the diagnostic trace,
run the same D81 screen for 30-60 seconds, and save the trace. Try a volume or
LED control during that period. Keep the tab visible and send the trace plus two
raw status samples. Compare browser p50/p95/max/timeouts, c64_net polling gaps,
frame rate and free RAM. Switch to CRT, wait at least 10 seconds and verify
50 fps and zero active-drive polling counters. Control actions can still wait
for a frame boundary; this change does not guarantee a response deadline.

## Guarded DATA-wait batching (current candidate)

The current candidate retains the tested Bluetooth build 7 Wi-Fi startup gate
and progressive Nimbus retries. Hardware confirmed cold boot with Nimbus off,
reconnection after machine switches, and three minutes of D81 operation with
Nimbus off and the web interface accessible. Additional intra-frame network
polling remains OFF. Dispatcher placement stays in flash, without explicit
ThinLTO; the clock remains 250 MHz.

The portable source adapter now batches complete seven-cycle iterations of the
uploaded `$06E4 BIT $4001 / $06E7 BEQ $06E4` loop. This does not turn off the
1581. Guards check the nine surrounding code bytes, accumulator, flags, last
branch metadata, absence of pending interrupts, and stable CIA input. Only the
built-in controller is eligible; timer-driven PB6/PB7 falls back to the original
interpreter. Batching stops before the next CIA alarm and never passes the
current drive catch-up target. IEC changes still reach the drive at the existing
synchronisation points. Partial iterations and all other instructions execute
normally. CIA read clocks and bus dummy-read state are preserved.

`drive1581.idle_loops` in `/api/status` counts batched iterations (seven emulated
cycles and two instructions each). It resets with drive reset/eject. The existing
`instructions` count includes those virtual instructions. A rising idle_loops
count confirms use of the shortcut; it is not a direct measure of time saved.
Other media keep the full-drive path disabled and report zero.

Validation:

- Differential tests compare complete CPU and CIA states, RAM, serial bus and
  board state with batching disabled. Cases include every stop clock over a
  600-cycle range, serial changes, input/DDR combinations, timer deadlines,
  PB timer-output modes, IRQ/NMI/reset, and changed code bytes.
- Native firmware bridge passes M-W/U3 and repeated mount/reset/eject allocation
  checks. All 20 Monkey Island frame captures and all 20 drive-RAM captures over
  10,000 frames are byte-identical to the original interpreter baseline. The
  shortcut accounts for 43,101,341 loop iterations in that run. C++ ASan/UBSan
  is enabled; the Rust code is not sanitizer-instrumented.
- Retry timing and actual Wi-Fi-gated retry-function tests pass. The Bluetooth
  implementation and retry-policy files are unchanged from the tested build.
- Master, Model B and C64 firmware builds pass. C64 link-time heap headroom is
  unchanged at 33,728 bytes; this is not live free RAM.

Run the focused differential suite with the configured Rust toolchain:

```sh
python3 experiments/c64-1581/portable/test_idle.py \
  --trx64 /path/to/pinned/TRX64 --output /tmp/retro-idle-validation
```

The output directory must be new. Hardware speed and responsiveness remain to
be measured: use the same Monkey Island D81 waiting/game screen, collect two
status samples at least ten seconds apart, and save a 60-second web trace. Then
switch to CRT and check that full speed returns and drive counters are zero.
Keep the tested nimbus-wifi-boot firmware for rollback. No changes published.

## Hardware acceptance of waiting-loop batching

The user elected to retain the guarded whole-loop optimisation on 2026-11-06.
Settled Monkey Island D81 samples reached about 61–62% PAL speed with drive_us
around 1.66 million per five-second window. Earlier inline-bus measurements were
54–56%; these were user-run workload comparisons, not a controlled benchmark.
A hostname-based trace spanned 4 minutes 4 seconds across two browser sessions,
with 108 successful status samples and no recorded errors/timeouts. Automatic
Nimbus retries were disabled. RAM returned to 30,888 bytes free after transient
dips. CRT switching restored 50 fps and zero drive execution/counters; later
samples returned to the same free-RAM value with continuous board uptime.

Earlier numeric-IP browser sessions lost access, including with retries disabled,
while emulation continued. Opening 8bitretro.local restored access without a
board restart. The exact cause remains unresolved; this is not evidence that
all Wi-Fi or automatic retry conditions have been validated. Brief timeouts
also coincided with outgoing Nimbus attempts in earlier traces.

## Next candidate: guarded individual wait instructions

Keep whole-loop batching and extend its guard to the final BIT and taken BEQ
that do not fit a whole iteration in a short drive slice. These two instructions
can now execute without entering the general opcode dispatcher. This targets
slice-boundary overhead; it does not accelerate unrelated ROM loading code.
The shortcut uses the same RAM signature, input, flag and interrupt checks.
It can finish one instruction beyond the requested stop clock, exactly as the
original interpreter does, but never crosses a CIA alarm. Branch metadata,
dummy reads, CIA read clocks and instruction counts remain identical.

`drive1581.idle_tail_instructions` counts these individually executed shortcut
instructions. `idle_loops` retains its meaning of complete batched iterations.
Both counters reset on drive reset/eject and are zero for CRT operation.
The original interpreter remains the differential-test reference. Tests assert
that both shortcut paths are actually exercised, including stop clocks inside
instructions, timer deadlines, serial changes, interrupt/reset and modified RAM.

This is a separate hardware candidate. Keep the previous 1581-idle-wait UF2 as
the accepted comparison build. Preserve Wi-Fi build 7/Nimbus policy, 250 MHz,
flash dispatcher, no explicit ThinLTO and extra network polling OFF. Use the
same settled D81 screen, sound setting and controller/retry state for comparison.

Candidate validation completed: differential suite passes; M-W/U3 and repeated
mount/reset/eject allocation checks pass; 20 frame captures and 20 drive-RAM
captures match the retained build byte for byte over 10,000 frames. The native
run exercises 4,330,800 individual shortcut instructions and 43,101,341 whole
iterations. Sanitized C ABI tests verify counter reset/eject. All three firmware
builds and combined packaging checks pass. Static C64 heap headroom is unchanged
at 33,728 bytes. No hardware gain is claimed before comparison samples arrive.

## Retained instruction-tail optimisation and next SRAM trial

On 2026-11-06 the user accepted retaining the individual-instruction shortcut.
Two mountain-screen samples measured 571 frames in 16.778 seconds (34.0 fps),
compared with approximately 31 fps for whole-loop batching alone. Drive work
was about 1.44 million microseconds per five-second window; free RAM was
30,888 bytes. CRT returned to 50 fps and all full-drive counters, including
idle_tail_instructions, reset to zero. This validates that comparison workload;
sustained web responsiveness for the new build was not explicitly confirmed.

The next separate experiment is `C64_1581_IDLE_SRAM=ON` (default OFF). It places
only the guarded waiting-loop helper into Pico's startup-copied SRAM section,
and prevents inlining of that helper. An outer PC check avoids calling it for
unrelated instructions. The instruction dispatcher remains in flash, unlike
the earlier rejected whole-dispatcher SRAM trial. Semantics of the two accepted
shortcuts are unchanged. Wi-Fi/Nimbus files are unchanged; additional network
polling remains OFF, no explicit ThinLTO is used, and clock stays 250 MHz.

ARM ELF inspection finds the built-in helper at 0x20007748, size 822 bytes, in a
file-backed SRAM segment with flash load address. Both general dispatchers stay
in flash. Alignment/placement increases linked SRAM use by 1,024 bytes; static
heap headroom is 32,704 bytes versus 33,728. Hardware free RAM should be compared
with this known cost rather than treating it as a leak. The placement test
bounds the helper and total cost and confirms its startup load image.

Differential tests using the outlined helper pass. Native M-W/U3, allocation
lifecycle and counter reset/eject tests pass. All 20 game frames and 20 drive-RAM
captures match the retained instruction-tail build byte for byte. All three
firmware builds, combined payload/slot checks and Wi-Fi gate tests pass.
Hardware speed benefit remains unknown: outlining introduces call overhead,
so retain this option only if comparable mountain-screen samples improve.
No changes have been committed or published.
