# D81 job-scan optimisation

D81 ROM job-scan candidate — 1a18f3e720d80881

Standalone C64 test firmware; based on diagnostic reference a98101def71eb0e9.
Same USB-stdio-off setting, memory and flash/lockout guards, early boot markers,
D81 batching, SID SRAM state/code, loading SRAM paths, Wi-Fi/Nimbus fixes and
web starting-screen source. Launcher source and stable release unchanged.
The launcher SHA is paired-artifact metadata; this standalone UF2 has no launcher.

Optimisation: guarded full iterations of DOS ROM B117..B126 job-queue scan.
Reads only RAM/ROM; preserves CPU flags, cycle and instruction counts, branch
bookkeeping and last bus byte. Stops before CIA alarms and IEC slice boundaries.
Unknown ROM bytes, pending interrupts, alternate CPU banks or loop exits fall
back to the interpreter. Option C64_1581_JOB_SCAN=ON (default remains OFF).
No audio gain, sample synthesis, output rate, buffer size or DMA changes.

Validation:
- Direct full-state differential comparisons over all X values, all Y values
  (selected deadlines), short slice tails, queue exits, flags, timer/IRQ events,
  clocks around 32-bit rollover, modified ROM, bank/IRQ/NMI rejection.
- D81 10,000 frames and CRT 10,000 frames; C/C++ ASan/UBSan, release Rust.
  All 100 framebuffer/C64 RAM/drive RAM snapshots identical to reference.
  PCM sample counts/hashes and IEC read/write counts/hashes identical.
  Mount/reset/eject and SID renderer lifetime checks passed.
- Seven alternating native benchmark rounds: loading-heavy first 3,000 frames
  median 589132 -> 559306 us (5.06% less time). Whole 10,000-frame run
  median 1145402 -> 1105113 us.
  Settled D81 median 548792 -> 547022 us.
  Initial separate seven-round comparison found 5.84% less loading time.
- Candidate with an additional loader LDA/BMI RAM-wait shortcut was correct
  but slower on this host; rejected. Saved here only for future investigation.
- Firmware build and SRAM relocation check passed: scheduler 1644 bytes,
  mapped reads 2104, combined 3748 < 4096 budget. Job helper stays in flash.
- Reset-entry ordering, memory guard, flash/lockout forwarding, network poll
  budget, Nimbus Wi-Fi gate and audio gain/packing tests passed.
- Web source untouched this task. Node.js unavailable, so the web-switch test
  was not rerun. No target hardware performance/audio/reset tests performed.

Limits: host timings do not predict RP2350/XIP timing. The benchmark covers
initial loading and a settled screen, not interactive scene transitions.
This candidate is not a claim that scene-loading audio interruptions are fixed.
The separate warm-reset problem remains unresolved.

Test: flash c64-d81-job-scan.uf2, power cycle, confirm image 1a18f3e720d80881.
Use D81, sound on at 10, turbo off, Nimbus retries enabled. Capture status
before/during/after the same load or scene transition; report audible dropouts
and web responsiveness. Power cycle if the known post-flash hang occurs.
Compare against reference-a98101def71eb0e9.uf2 with matching settings.
Restore combined firmware from ../c64-memory-guard/restore-combined-usb-diagnostic.uf2.

## Audio checkpoint: 10 November 2026

The SID state/code, batching and loading SRAM improvements are already recorded
in the preceding experimental commits. Retain image `1a18f3e720d80881` as the
reference for further loading-audio work.

The optional `C64_1581_SERIAL_WAIT` experiment remains default OFF. Its guarded
CLOCK waits passed differential tests and 10,000-frame D81/CRT comparisons,
with identical memory/display snapshots and PCM/IEC traces. Hardware candidate
`3bb77c71ef261c1e` sustained 50 fps for 55.061 seconds of settled gameplay with
no added underruns; the user reported responsive web controls but unchanged
sound interruptions during loading. It is retained for reproducible testing,
not promoted as an audio improvement. Status exposes serial loop/tail counters.

The later arithmetic-only controller polling shortcut and three controller-event
batching variants were rejected after native benchmarks showed no gain. Their
code is excluded from this checkpoint. Warm-reset diagnosis remains separate;
this checkpoint makes no claim that the intermittent physical-reset fault is fixed.
