# D81 loading-path SRAM experiment — 9 November 2026

Reference: pushed checkpoint cbf187f, image 4020d3c9303e5a06. New candidate: 59c70b444bed15d3. Existing reference UF2s remain immutable. Stable release unchanged.

## Diagnosis and scope

The reference sustains 50 fps in tested gameplay, but hardware reports large underrun increases during disk loading. Samples are generated according to emulated raster progress, while DMA consumes at a fixed real-time rate. The 4096-sample ring covers only about 93 ms. Increasing capacity cannot compensate for sustained below-real-time emulation over a long loading interval. We therefore target the active drive workload before changing audio buffering, pitch, sample generation or underrun concealment.

C64_1581_LOADING_SRAM (default OFF) relocates two compiled Rust sections: Drive1581::run_cycles (1486 bytes) and built-in Bus1581::read_mapped (2104 bytes). The former includes scheduling/instruction-boundary work; the latter services mapped CIA/FDC register reads exercised by loading routines. Total selected code is 3590 bytes, bounded by a 4096-byte build guard. General opcode dispatch, mapped writes, host-FDC paths and tables remain in flash. No instructions, polling cadence, emulated clocks, register side effects or PCM arithmetic are changed. This is a narrower experiment than the earlier 9 KiB dispatcher trial, which did not establish a useful benefit.

The linker places mapped reads at 0x20007dc0 and run_cycles at 0x200085f8. Linker __end__ moves from 0x200744d8 to 0x200750d8, a net 3072-byte increase after section alignment/layout. Heap limit remains 0x2007c000. Runtime heap use must be checked on hardware. SID code/state in SRAM, 16-line drive batching, 250 MHz clock, 5 ms network polling and Nimbus build 8 remain enabled. Launcher payload is byte-identical to the reference (UF2 SHA a7a8edcfbceacbcdfd457b13391920d64c1f8affb4be6c4973c1663d5719d83f).

## Validation

- Firmware builds and all image slots fit.
- Archive checks confirm both selected code sections are byte-identical before/after relocation, final symbols are in SRAM, and the built-in dispatcher remains in flash. Missing/duplicate sections and an oversized inventory fail closed.
- Host C/C++ ASan/UBSan: M-W/U3, repeated mount/reset/eject, 10000 D81 frames, 10000 CRT frames and SID renderer lifecycle checks pass. All 100 screen/memory snapshots, ordered serial hashes/counts and PCM hashes/counts match the reference. Rust is not sanitizer-instrumented. Host execution cannot reproduce ARM SRAM/flash latency; these runs verify semantics, not a loading-speed benefit.
- Audio DMA shutdown, network poll budget, Wi-Fi retry gating, launcher fallback and linked SRAM partition caller checks pass.

No audio-interruption fix is claimed before hardware results. The candidate may improve loading throughput, but it may not be enough to maintain 50 fps throughout a load. Intermittent warm reset remains unresolved.

## Hardware comparison

Use the same D81 scene transition on reference and candidate when possible. Keep sound on at volume 10, Nimbus off/retries enabled, turbo off and 8bitretro.local. Flash/power cycle and verify candidate image 59c70b444bed15d3. Capture status immediately before the transition, during loading if feasible, and after it settles. Report approximate loading duration and audible interruptions. Capture a second settled sample 20–30 seconds later to separate loading underruns from gameplay. Check web response during loading and free RAM. Finally check CRT. Avoid comparing aggregate counters from different boots; use within-run deltas.

## Hardware checkpoint

User tested image 59c70b444bed15d3 with sound on at volume 10 and Nimbus off/retries enabled. Settled D81 window: 251 frames/5019922 us (~50 fps), core 0 72%, drive 615504 us (2.45 ms/frame), about 25% pacing headroom. During scene loading: 132 frames/5001524 us (26.39 fps), core 0 99%, drive 2628374 us (19.91 ms/frame). CPU timing includes drive calls and must not be summed with drive timing. Wi-Fi remained on its original join; API responses continued. Free RAM recovered from 15888 bytes with four connections to 22392 with two. User reports everything seems better, with audible audio suffering specifically as loading saturates CPU. This is qualitative improvement, not a matched reference loading-speed measurement.

Checkpoint preserves full-speed tested gameplay and all prior diagnostics. Post-flash automatic reboot also intermittently hangs with all LEDs green and no USB firmware drive; power cycling boots the written image. That startup/reset problem remains separate and unresolved. Active loading still needs optimization.
