# C64 SID renderer SRAM-state experiment — 9 November 2026

Candidate image: 2341747e7259bc95. Baseline e2a899f4c3ea3ead is checkpointed at e7624bb on experiment/c64-1581. Original reference and earlier candidates remain preserved. Nothing has been pushed or released as stable.

## Rationale and scope

With the 16-line drive scheduler, hardware now delivers approximately 48.6–48.9 fps. Sound-on samples show SID generation consuming ~4.4 ms per emulated frame and ~1250 underrun samples per second. The C64 global C++ allocator places DigitalRenderer (voice envelopes, phases, filter state and raster history) in external PSRAM. Its frequently accessed mutable state can contend with rendering and other emulation traffic.

C64_SID_FAST_STATE=ON adds a class-specific allocator only to the disposable generated SID source. It obtains DigitalRenderer from the internal malloc heap, with a matching class-specific delete. Allocation failure reports a clear panic. All other C64 object allocations, tables, SID algorithms, sample cadence, audio DMA, output volume, ring size and waveform arithmetic remain unchanged. Drive batching stays at 16, network polling stays as before, and detailed drive profiling stays off. The option defaults OFF and applies to the experimental full-1581 generated core.

The ARM binary allocates 1476 bytes for the renderer (1512 on the 64-bit host). It calls c64_sid_state_alloc -> SDK wrapped malloc, not the PSRAM allocator. c64_audio now reports sid_state_bytes and sid_state_sram; expect 1476 and true on this candidate. This consumes about 1.5 KiB more SRAM plus allocator bookkeeping and releases the corresponding PSRAM allocation. SID code remains in flash, so this isolates data placement.

## Tests

Baseline and candidate each ran the M-W/U3 probe, mount/reset/eject allocation/network checks, 10,000 D81 frames and 10,000 CRT frames with C/C++ ASan/UBSan. Rust is not sanitizer-instrumented. All 100 screen/C64-RAM/drive-RAM snapshots match byte for byte. Ordered serial transaction counts and 64-bit hashes match.

PCM output counts and rolling hashes match exactly (both stereo channels included):
- Probe: 1270957 samples, 357e947da4988073.
- D81: 9067827 samples, bfad6962ef231187.
- CRT: 8819991 samples, f17f1135198ccf5b.

After each run the real SID preference/destructor path was exercised through twenty off/on cycles and final off. Host allocation tracking returned to zero. The existing audio-DMA shutdown, network budget, Wi-Fi gate, launcher fallback and SRAM ROM-call placement tests passed. Firmware slot checks passed. The packaged launcher payload is byte-identical to the reference.

Host RAM does not emulate the Presto's PSRAM latency or contention; no native speed improvement is claimed. The hardware test must establish whether state placement saves enough time for sustained 50 fps and reduces underruns. This is not an audio waveform/clipping fix or a warm-reset fix.

## Hardware test

Power cycle after flashing; verify image 2341747e7259bc95. Load the same D81 and wait for the mountain screen. Keep turbo off, sound ON at volume 5, Nimbus off with retries enabled, and use 8bitretro.local. Send two status samples 20–30 seconds apart and report sound/web responsiveness. Confirm c64_audio.sid_state_sram=true and sid_state_bytes=1476. Compare fps, SID time per frame and underrun delta against the batching checkpoint. Check CRT and later scene loading after the initial comparison.

## Reset evidence retained

The preceding batching run failed warm reset: seven green LEDs on the first and second presses, one blue on the third, five blue subsequently, and two white also observed after further presses. Web unavailable; power cycling restored it. Follow-up reported eventual USB firmware-download mode. This remains unresolved; this candidate keeps the exact same launcher diagnostics and introduces no reset-path changes.

## Hardware results and checkpoint

Tested image 2341747e7259bc95 with Nimbus off/retries enabled, 250 MHz and unchanged launcher. Sound-on volume 15 mountain samples averaged 48.91 fps over 24.042 s; underruns grew by 23235 (966/s). SID cost was 3.88–3.95 ms/frame. Compared with the earlier batching run this is encouraging, but volume and scene differences prevent a controlled speedup claim.

CRT sustained 50 fps, core 0 60%, SID 2.14 ms/frame; the user confirmed smoother audio and responsive web. D81 gameplay and a new scene were reached successfully. A heavier gameplay interval averaged 48.35 fps over 26.786 s, with 39281 additional underruns (1466/s, sound off but DMA running). The new scene measured 49.88 fps. Thus sustained 50 fps across scenes is not achieved. SID SRAM placement is confirmed at 1476 bytes, with approximately 1.5 KiB additional internal heap use. Wi-Fi remained joined throughout; HTTP expiry counters occasionally increased. No new reset fix is claimed.

Checkpoint retains the candidate and all prior reference artifacts. Next experiments must retain exact PCM/bus/frame behavior and preserve launcher, network and Nimbus code.
