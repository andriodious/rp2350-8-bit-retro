# Guarded ROM delay-loop batching — 9 November 2026

Reference checkpoint: 8908500, tested loading-SRAM image 59c70b444bed15d3. Earlier full-speed checkpoint cbf187f/image 4020d3c9303e5a06 remains preserved. Candidate: 07ec6448def021f4. No stable-release update or push is included.

## Evidence and implementation

A host-only instruction histogram found 2,091,567 executions each at AD37 (DEX) and AD38 (BNE AD37) during the D81 test. Other large hotspots include the ROM job scan around B117 and the disk-status polling around C969. Instruction counts are not cycle-cost percentages. The histogram flag in firmware/test.py never instruments hardware firmware.

C64_1581_DELAY_LOOP (default OFF) enables a guarded shortcut for the exact ROM bytes CA D0 FD AA at AD37–AD3A. It batches complete five-cycle taken DEX/BNE iterations, keeps X nonzero, and leaves the final decrement/not-taken branch to the interpreter. It preserves X, N/Z flag representations, opcode/interrupt bookkeeping, instruction count and the final dummy-read bus value. No emulated time is removed. Unknown ROM bytes, jammed CPU, banked fetches, pending interrupts, queued CIA IRQ events or insufficient slice time use the interpreter. The batch ends before the next CIA alarm and never crosses the caller's synchronization target. Existing serial reads/writes still catch up normally.

The helper remains a separate 336-byte flash function for each instantiated bus type. An initial inlined version exceeded the existing 4 KiB loading-section budget and was not packaged. The final scheduler remains in SRAM (1564 bytes); mapped reads remain 2104 bytes. The loading-section total is 3668 bytes. Linked __end__ remains 0x200750d8, identical to the loading-SRAM reference. Existing SID SRAM state/code, batching, 5 ms polling, 250 MHz, audio DMA and launcher/Wi-Fi/Nimbus fixes are unchanged.

## Validation

Direct differential tests compare complete CPU, interrupt, CIA, RAM and bus-related state against the interpreter for every X value, slice lengths 1–65, continuing irregular slices, clocks on both sides of the 32-bit boundary, real CIA timer alarms and IRQ delivery. Positive control confirms a batch occurs; altered ROM bytes, pending IRQ/NMI, jammed CPU, bank window and X=1 reject it. Existing DATA-wait tests pass too.

C/C++ ASan/UBSan: M-W/U3 probe, mount/reset/eject, 10000 D81 frames, 10000 CRT frames and repeated SID renderer lifecycle tests pass. All 100 reference screen/memory snapshots, serial transaction counts/hashes and PCM counts/hashes match exactly. Rust is not sanitizer instrumented. Firmware builds and slot checks pass; loading SRAM placement/byte-preservation, launcher SRAM partition path, audio shutdown, network poll budget, Wi-Fi retry gate and launcher fallback tests pass. Launcher UF2 digest remains a7a8edcfbceacbcdfd457b13391920d64c1f8affb4be6c4973c1663d5719d83f, and its packaged payload matches the reference.

Seven rounds of rotated native benchmarks, without sanitizer or PC profiling, yielded these median microseconds:

| Workload | Reference | Candidate |
| --- | ---: | ---: |
| D81 first 3000 frames (includes loading) | 604637 | 580608 |
| D81 remaining 7000 frames | 545299 | 543435 |
| D81 total 10000 frames | 1150329 | 1124964 |
| CRT total 10000 frames | 744724 | 743267 |

The loading-inclusive interval is 3.97% shorter, total D81 2.20% shorter; steady/CRT differences are small. This is host evidence, not a prediction of RP2350 performance, and the initial loader is not identical to every later scene transition. No claim of eliminated audio starvation is made.

## Hardware test and outstanding issues

Power cycle after flashing and verify image 07ec6448def021f4. Repeat the same scene transition, sound on at volume 10, turbo off, Nimbus off/retries enabled, using 8bitretro.local. Capture before/during/after loading status and a second settled sample. Compare elapsed loading time and within-run underrun growth, not absolute counters from different boots. Check full-speed settled gameplay, CRT and web responsiveness.

Loading-time audio starvation, intermittent warm/post-flash reboot failures and occasional HTTP expiry counters remain outstanding. The user reported all-green LEDs after some post-flash reboots, with power cycling restoring boot; this change does not address reset behavior.
