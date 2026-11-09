# SID sample routine in SRAM — 9 November 2026

Tested state-placement candidate 2341747e7259bc95 is checkpointed at 61e5828 on experiment/c64-1581. New hardware candidate: 4020d3c9303e5a06. Stable release and all reference UF2 files are unchanged; no push performed.

The heavier gameplay scene averaged 48.35 fps, with SID generation about 4.22 ms/frame. This isolated experiment places DigitalRenderer::calc_single_sample in the SDK time_critical SRAM section and prevents inlining. The ARM linker places its 784 bytes at 0x20000150. It retains the existing SRAM renderer state, exact waveform/filter arithmetic, sample cadence, DMA, 16-line drive batching and 250 MHz clock. C64_SID_HOT_CODE defaults OFF. The host build uses noinline without the ARM section attribute, since macOS has a different object format.

This trades roughly 0.8 KiB of internal RAM for reduced flash instruction fetching/cache competition. Native timings cannot establish that gain. The hardware results below establish full speed in the tested scenes, not all possible workloads.

Validation: C/C++ ASan/UBSan runs passed the M-W/U3 probe, mount/reset/eject checks, 10000 D81 frames, 10000 CRT frames and repeated real SID renderer preference/destructor cycles. All 100 screen/C64-RAM/drive-RAM snapshots match the checkpoint manifest; PCM counts/hashes and ordered serial counts/hashes match exactly. Rust is not sanitizer instrumented. Audio shutdown, network polling budget, Nimbus Wi-Fi gating, launcher fallback and linked SRAM partition-call placement checks passed. All firmware slots fit. Packaged launcher payload matches the prior candidate byte-for-byte; its UF2 SHA256 remains a7a8edcfbceacbcdfd457b13391920d64c1f8affb4be6c4973c1663d5719d83f.

Hardware procedure: flash, power cycle, verify image 4020d3c9303e5a06. Return to the same heavier D81 gameplay scene, initially with sound off as in the 48.35 fps baseline, Nimbus off/retries enabled and turbo off. Capture two settled samples about 20–30 seconds apart. Then check sound-on volume 15 and CRT regression. Keep web responsiveness under observation. Warm-reset failure remains unresolved and this change does not target it.


## Hardware checkpoint results

Image 4020d3c9303e5a06, 250 MHz, turbo off, Nimbus switched off with automatic retries enabled:

- D81 settled gameplay: uptime 196361–223660 ms, frames 6258–7623, giving 50.002 fps over 27.299 s. Sound off, volume 10; DMA running. Underruns 3123346–3123437: 91 additional, or 3.33 missing samples/s. Core 0 77%; about 1.05–1.08 seconds per five-second profile window spent pacing. SID cost 1.82–1.88 ms/frame.
- D81 sound on at volume 10: 251 frames in 5019963 us, core 0 78%, approximately 20% pacing headroom. User confirmed the intervening underrun increase accompanied disk loading between scenes. This sample alone does not establish a settled sound-on underrun rate.
- CRT sound on at volume 10: uptime 406047–421267 ms, frames 16121–16882: 50.000 fps over 15.220 s. Underruns 3672969–3673082: 113 additional, or 7.42 missing samples/s. Core loads 59%/32%; drive inactive. Wi-Fi remained on the original join, with no additional errors/expiries between the two CRT samples.

The current hardware evidence supports full-speed operation in the tested D81 gameplay and CRT scenes. The prior heavy-scene state-only candidate averaged 48.35 fps with core 0 near 98%. The code-placement change shows a substantial improvement, but broader game compatibility and longer runs remain useful. Occasional HTTP connection expiries still occur; status responses do not alone prove every web-page operation remained responsive.

Outstanding: audio starvation during disk loading, occasional small steady-state underrun increments, and intermittent warm-reset failures. No reset fix is claimed. Launcher diagnostics, 5 ms polling, Wi-Fi/Nimbus behavior and reference firmware remain preserved. Checkpoint is local on experiment/c64-1581; no push or stable-release update is included.
