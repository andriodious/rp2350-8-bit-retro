# D81 bounded scanline-flush candidate — 9 November 2026

Hardware candidate C64 image: e2a899f4c3ea3ead. It builds on checkpoint 583a200e080a39ab (commit 1bb0b41), with the later optional profiler retained but disabled. Original reference 84aa7becf45c48d9 is preserved. Launcher SHA256 remains a7a8edcfbceacbcdfd457b13391920d64c1f8affb4be6c4973c1663d5719d83f.

## Change

C64_1581_FLUSH_LINES=16 reduces otherwise-unobserved drive catch-up calls from every raster line to every sixteen lines. Every CIA2 serial read/write still collects all pending cycles and catches the drive up before the access. VBlank and the frame safety limit always force catch-up. Drive CPU instructions, CIA alarms, interrupts and disk-controller execution are unchanged. No cycles are discarded; this is bounded scheduling, not drive suspension or an idle-loop-specific hack. Network polling remains at every raster line under its existing 5 ms time budget. The change is confined to the optional full-1581 adapter. Default flush interval remains 1; accepted experimental values are 1,4,8,16. No launcher/radio/clock changes.

Six alternatives were explored against the checkpoint: moving idle detection before alarm work; straight-line idle tail execution; both combined; and 4/8/16-line scheduling. The first three produced no host benefit and are NOT included in the candidate. Their generated-source experiment recipe is benchmarks/wait_variants.py. Only the scheduling change is selected.

## Native benchmark

Apple Silicon host, C++ -O2 without sanitizers, Rust release opt-level=2. Each process runs the real supplied D81 or CRT for 10,000 game frames. The steady interval is frames 3000 through 10000. Five rounds rotated variant order; no simultaneous benchmark processes. Baseline and selected candidate also ran five CRT rounds; the other variants each passed one full CRT run. Timing includes output snapshots and host clock instrumentation; it is not a model of Cortex-M33, XIP, PSRAM, LCD DMA or radio contention.

| Variant | Median steady D81 time, 7000 frames |
|---|---:|
| Checkpoint | 630.516 ms |
| Earlier idle detection | 636.310 ms |
| Straight-line tails | 648.740 ms |
| Combined idle changes | 642.980 ms |
| 4-line scheduling | 562.857 ms |
| 8-line scheduling | 547.464 ms |
| 16-line scheduling | 544.120 ms |

Selected candidate reduces median steady time by 13.7% (about 15.9% higher native throughput). Total 10,000-frame D81 median falls from 1257.258 to 1138.154 ms (~9.5%). CRT steady median is 501.190 versus 504.175 ms (~0.6% difference); no meaningful CRT gain is claimed. The 8/16-line difference is small and ranges overlap; sixteen had the lowest median, but hardware may rank them differently. This is the fastest of the tested candidates, not a claim of a globally optimal implementation or guaranteed full-speed Presto execution.

## Correctness checks

All six variants matched 60 D81/CRT screen and drive-memory snapshots; the three idle-code variants also passed the interpreter differential suite. Final one-line baseline and sixteen-line candidate were rebuilt with C/C++ AddressSanitizer/UBSan and additional bus tracing. Both passed M-W/U3 execution, repeated mount/reset/eject allocation and network-hook checks, and 10,000 frames of each supplied game image. Rust was not sanitizer-instrumented.

All 100 snapshots match byte for byte: twenty each of D81 screen, D81 C64 RAM, drive RAM, CRT screen and CRT C64 RAM. Serial transaction counts and rolling 64-bit hashes match:

- Probe: 4092 reads, 2150 writes; hash 1b12e37b329e4023.
- D81 game: 2826431 reads, 139993 writes; hash 5403e947b607b14f.
- CRT: no drive transactions.

These hashes cover ordered read results and write values, not a physical logic-analyser capture. The checks cover these supplied images and probe, not all possible D81 loaders. No binaries from the supplied game were copied into the repository.

Existing network poll-budget, Wi-Fi retry-gate, launcher fallback and SRAM ROM-call placement tests passed. Firmware slot bounds passed. The packaged launcher payload is compared byte for byte with reference. Both prior reference and checkpoint firmware digests are verified unchanged.

## Reproduction / hardware

Use firmware/test.py --benchmark --crt PATH --flush-lines N for each native build, then benchmarks/repeat.py with --builds, --rom, --d81, --crt and --output. Omit --benchmark and add --bus-trace for sanitizer validation. The packaged benchmark-results.json records the performed runs, and host-validation.json records image/snapshot hashes. Explicitly configure CMake with C64_1581_FLUSH_LINES=16 and C64_1581_DRIVE_PROFILE=OFF for the hardware candidate; OS_CMAKE_ARGS alone does not update existing caches.

Power cycle after flashing; verify e2a899f4c3ea3ead. Use the same settings: sound off, Nimbus off with retries enabled, turbo off, 8bitretro.local. Send two settled mountain-screen status samples 20–30 seconds apart and report web responsiveness. Compare with checkpoint's approximately 39.33 fps. Then switch to CRT and confirm approximately 50 fps with drive inactive. The earlier profiling build may be skipped for this test. Warm-reset reliability remains unresolved and is not claimed fixed.

## Hardware checkpoint

Image e2a899f4c3ea3ead sustained 1332 frames over 27.266 seconds: 48.85 fps, with 3.21–3.30 ms/frame drive cost. CRT returned to 50 fps with drive inactive. The user confirmed the web page remains responsive. With sound enabled at volume 5, a later 20.971-second interval delivered 1019 frames (48.59 fps) and underruns increased by 26211 (~1250/s). Audio is substantially improved but still choppy. Wi-Fi remained on its original join. D81 gameplay/scene-loading validation and intermittent warm-reset diagnosis remain outstanding; this is not a stable release.

Subsequent hardware reset report on the batching run: first press seven green LEDs, second seven green, third one blue, subsequent presses five blue, with two white also observed after multiple presses. Web unavailable; power-on reset restored operation and web access. This supersedes any inference that the performance build resolved warm-reset reliability. USB presence was not specified for this event.

Follow-up: the same reset sequence eventually reached USB firmware-download mode. No cause is inferred from this observation.
