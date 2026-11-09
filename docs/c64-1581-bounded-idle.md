# D81 bounded idle arithmetic experiment — 9 November 2026

Candidate C64 image: `c15dd5a068a86233`.
Reference: launcher-fallback-report image `84aa7becf45c48d9`.
Launcher UF2 SHA256 and payload unchanged: `a7a8edcfbceacbcdfd457b13391920d64c1f8affb4be6c4973c1663d5719d83f`.

Only D81 algorithm change: bound a complete waiting-loop batch to at most UINT32_MAX cycles, calculate its loop count and elapsed cycles with 32-bit arithmetic, then add to the existing 64-bit emulated clock and counters. An unusually large interval is completed by subsequent batches; it is never discarded. Seven-cycle loop semantics, IRQ guards, instruction-tail handling, CIA alarm boundaries and IEC synchronisation remain unchanged. General CPU dispatcher remains in flash; helper remains in SRAM. Clock is still 250 MHz. No SID bypass, no frame skipping, no drive suspension, no changes to the radio or launcher diagnostic behaviour.

Compiled helper shrank from 822 to 774 bytes at 0x20007718. Assembly shows a shorter 32-bit constant-divisor calculation. No claim of measured hardware speedup yet. It may be small or offset by code-layout effects. This alone is not expected to guarantee full-speed D81.

Checks passed: original-interpreter differential waiting-loop suite, arithmetic batch boundaries around UINT32_MAX, firmware build and combined slot bounds, exact launcher payload/hash comparison, existing network poll-budget and Wi-Fi reconnect-gate tests, launcher SRAM call placement and whitespace check. Full Monkey Island host regression now passed using the supplied D81 (SHA256 fcdc6322109fa286fab039381cc0c5321c54b6368cf3d226cc7ecafe2bb47995). Candidate and the previous 64-bit batch arithmetic each completed 10,000 game frames; all 20 screen snapshots and all 20 drive-RAM snapshots match byte for byte. Both passed the M-W/U3 execution probe and repeated mount/reset/eject allocation and network-hook checks under AddressSanitizer/UBSan for the C/C++ harness. Rust was compiled normally, not sanitizer-instrumented. Host allocation accounting returned to baseline on eject. Snapshot hashes are recorded in host-validation.json beside the UF2. Hardware speed and web responsiveness validation remain pending.

## Hardware comparison

If practical, collect two mountain-screen /api/status samples 10 seconds apart on the reference first. Use Monkey_Island_v1.2.d81, turbo off, sound off and the same Nimbus/retry settings for both builds. Earlier 82% results are useful context but are not a matched baseline for this precise run.

Flash candidate, power cycle (post-flash warm startup remains under investigation), verify C64 image ID, load the same D81 and wait for the mountain screen to settle. Send two samples 10 seconds apart and note whether web controls respond normally. Compare frames/window_us and drive_us per emulated frame; comparing drive_us alone can hide increased throughput.

Then switch to the CRT version, wait at least 10 seconds and capture status: expect drive inactive, drive_us zero and roughly 50 fps. Test sound separately after the matched comparison. If Reset fails, retain the launcher diagnostic procedure: leave powered, report static/blinking LED colour/count and whether USB serial or a firmware drive appears.

Neither this candidate nor the preceding diagnostics have been published as a stable release. The reference UF2 is preserved.

## Hardware result — 9 November 2026

Matched mountain-screen measurements gave approximately 38.85 fps for this candidate and 38.82 fps for reference 84aa7becf45c48d9: no meaningful gain. Reference retained; bounded arithmetic removed from the next experiment, which instead fuses shortcut tails to reduce repeated validation/dispatch work.
