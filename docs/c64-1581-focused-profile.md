# Focused D81 profiling build

The tested fused-idle candidate is checkpointed at 1bb0b41 on experiment/c64-1581. Its preserved UF2 identifies as 583a200e080a39ab. Reference 84aa7becf45c48d9 remains unchanged. No push or stable release is part of this checkpoint.

Enable C64_1581_DRIVE_PROFILE=ON explicitly in the C64 CMake cache (OFF by default). The existing build script only uses OS_CMAKE_ARGS when first creating a build directory. The generated Rust feature drive-profile instruments the existing execution path; there are no new emulation shortcuts, radio changes, or changes to the launcher.

## Measurements

/api/status gains drive_profile while a drive is mounted and has run. Counters are cumulative since mount/reset and clear on eject; use deltas between two settled samples from the same tape generation:

- calls: drive run_cycles calls considered for sampling.
- samples: calls selected by a deterministic pseudorandom sampler (approximately 1/256).
- sync_us: sampled run_cycles setup, slice bookkeeping and remainder. This does NOT include the outer bridge's IEC folding or callbacks.
- alarm_us: sampled instruction-boundary alarm processing and IRQ event draining.
- idle_us: sampled idle eligibility checks and fused waiting-loop shortcut.
- cpu_us: sampled interpreter execution, including mapped CIA/FDC work invoked by instructions.

These four times partition the sampled run_cycles intervals, not all execution. Their sum and proportions are useful; do not add them to drive_us, or compare their absolute totals to unsampled drive_us. clock reads, branches, quantisation, cache/layout changes and sampling variance affect results. This is a diagnostic build, not a performance candidate. Existing c64_profile and conversion_us remain available; conversion runs on another core and is not additive to core-0 emulation time. No memory-contention conclusion is assumed.

The sampler runs only on emulation core 0, outside IRQs. Time differences use wrapping 32-bit microseconds, accumulated into 64-bit totals. Unsampled calls do not read the clock at stage boundaries. Profile state resets on mount/reset/eject. No network service or API call occurs inside an emulation instruction.

## Validation and hardware procedure

Sampler accounting, reset, sampling frequency and clock wrap passed with a deterministic clock. The instrumented waiting-loop differential tests passed, including 9,216 successive-slice comparisons. The instrumented C/C++ ASan/UBSan host harness completed the probe, repeated mount/reset/eject checks and 10,000 game frames; all 20 screen and 20 drive-RAM snapshots matched the fused candidate byte for byte. Rust was not sanitizer-instrumented. The instrumented C ABI lifecycle counter test also passed. Existing poll-budget, Wi-Fi retry-gate and launcher fallback tests passed.

After flashing, power cycle. Keep sound off, Nimbus off, retries enabled, turbo off, and use 8bitretro.local. Load the D81, wait for the mountain screen to settle, and send two full status samples 20–30 seconds apart. Retain the existing renderer and other settings. These measurements will select the next optimisation rather than claim a speedup. CRT may be checked afterwards: expect no drive_profile object and zero drive_us after eject. Reset failure investigation stays separate.
