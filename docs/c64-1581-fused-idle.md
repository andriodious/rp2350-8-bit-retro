# D81 fused waiting-loop candidate — 9 November 2026

C64 image: `583a200e080a39ab`. Reference remains `84aa7becf45c48d9`.

The shortcut now finishes multiple waiting-loop portions within one validated call instead of returning through the dispatcher after each branch/BIT tail. It retains the original instruction effects, timestamps, input and code guards, and returns at slice/alarm boundaries. It does not cache validation across IEC calls or suspend the drive. Original 64-bit batch arithmetic is restored: the previous bounded-arithmetic candidate matched reference throughput and offered no measurable hardware benefit.

The helper occupies 920 bytes in SRAM at 0x20007718 (reference: 822 bytes). Dispatcher placement and the 250 MHz clock are retained. Launcher SHA256 is unchanged: a7a8edcfbceacbcdfd457b13391920d64c1f8affb4be6c4973c1663d5719d83f. Combined firmware launcher payload was compared byte for byte with the preserved reference. D81, Wi-Fi/Nimbus and startup diagnostics remain included; stable release is unchanged.

## Validation

- Differential tests against the original interpreter passed for stopping boundaries, CIA timers/interrupts, input/flag values, changed loader code and instruction tails.
- Added 9,216 successive-slice comparisons, including changing DATA and clocks around the 32-bit boundary.
- Supplied Monkey Island D81 completed 10,000 emulated frames. All 20 screen and 20 drive-RAM snapshots exactly match the original 64-bit reference run.
- M-W/U3 execution, repeated mount/reset/eject allocation accounting and network-hook checks passed. C/C++ host harness used AddressSanitizer/UBSan; Rust was not sanitizer-instrumented.
- Firmware build and slot checks passed, as did actual network poll-budget, Wi-Fi reconnect-gate, launcher fallback mock and SRAM ROM-call placement tests.

Snapshot hashes are in host-validation.json beside the firmware. These checks establish host-level behavioural equivalence, not a measured hardware speedup or a warm-reset fix.

## Hardware comparison

Power cycle after flashing. Verify image 583a200e080a39ab, then use the same D81, turbo off, sound off, Nimbus off with retries enabled. After the mountain screen settles, capture two status samples 10–20 seconds apart and check web responsiveness. The matched reference delivered about 38.8 fps (76–77% reported), with drive cost about 6.8 ms per emulated frame. Compare frames/window_us and drive_us/frames.

Then switch to CRT, wait ten seconds and confirm roughly 50 fps with drive inactive and drive_us zero. No additional reference flashing is required for the initial comparison. Warm-reset diagnosis remains open; preserve LED/USB observations if it recurs.

## Hardware checkpoint

Image 583a200e080a39ab: settled D81 windows 195/5.014983 and 198/5.002015 frames/s; 661 frames over 16.806 seconds (39.33 fps). Drive cost 6.21 then 6.13 ms/frame, versus reference approximately 6.82 ms/frame. Overall improvement is small (~1.3%); not near full speed. Two CRT windows delivered 50 fps with drive inactive and zero drive time. Warm reset after D81-to-CRT passed and the website returned. Intermittent earlier reset failures remain unresolved. Wi-Fi stayed joined; two HTTP errors/expirations appeared during the switch, then remained unchanged.

Reference UF2 and launcher remain preserved. This checkpoint includes the accumulated launcher, identity, watchdog/audio diagnostic and D81 changes; it is experimental, not a stable release.
