# Spectrum 48K media loading

This isolated candidate starts from the hardware-tested `a2340cb2c0c69fc5` source. It retains the 50 fps CPU-loop/audio optimisations, asynchronous eight-slot renderer, fault recovery, Wi-Fi and Nimbus startup fixes. The verified `a61925553fc3ddd2` Spectrum and clean combined BBC/C64 artifacts are not modified.

## Formats

- `.sna`: existing 48K snapshots.
- `.z80`: 48K v1, v2 and v3 snapshots, compressed or uncompressed; all three RAM pages are required. 128K, SamRam, 16K and snapshots with paged peripheral ROMs are rejected. Register and memory validation completes before the running machine changes.
- `.tap`: standard Spectrum tape blocks, played at original pilot, sync and bit timings.
- `.tzx`: standard and turbo data (10/11), pure tones (12), pulse sequences (13), pure data (14), direct recording (15), pause/stop (20), loops (24/25), jumps (23), calls/returns (26/27), 48K stop (2A), signal level (2B), and metadata (21/22/30/31/32/33/35/5A).

CSW (18), generalized data (19), selection menus (28) and other unsupported TZX blocks produce an explicit error rather than silently skipping recording data. Maximum image size is 1 MiB; TZX permits up to 8192 blocks and eight nested loops/calls. A zero-time control cycle stops with an error, keeping the web interface usable. Content must be compatible with a 48K Spectrum and the included OpenSE BASIC ROM.

## Use

Choose a file in the browser, drop it onto the page, or select it from the SD card. ZIP selection accepts all four formats. Snapshots start immediately. Inserting a tape resets to BASIC, types `LOAD ""` using OpenSE's letter-entry keyboard, then starts playback. Loading takes the original tape time.

The tape panel reports state and block position. **Stop** pauses without losing position; **Play** resumes (also after a tape's stop command); **Rewind** returns to the start without resetting the Spectrum; **Restart and load tape** resets BASIC and repeats the automatic load. The existing Boot/Reset controls still restore built-in Elite.

The included `tape-test.tap` and `tape-test.tzx` are original small BASIC programs. They should print `TAPE OK`. They are useful for checking browser uploads and SD loading without downloading a game.

## Implementation

Tape pulse lengths use emulated Z80 T-states. Execution is split at signal edges only while playback is active; no per-tick tape callback is added to the normal CPU loop. EAR is sampled at port FE reads. The same beeper and video paths continue running during loading.

PSRAM layout: emulator workspace below 256 KiB, mounted-image staging at 256 KiB, network staging at 512 KiB, and a retained 1 MiB tape copy at 2 MiB. Incoming uploads cannot corrupt a playing tape. Core 0 owns tape/snapshot changes at the existing frontend safe point. This candidate requires at least 3 MiB PSRAM (Presto has 8 MiB). No flash-writing or reset code was changed.

## Validation

- ASan/UBSan: snapshot versions, register/bank mapping, raw/RLE decoding, truncation and overrun rejection.
- End-to-end TAP and standard TZX loads: unmodified ROM reads EAR pulses and runs a BASIC POKE program successfully.
- TZX pulse lengths/levels, partial bytes, loops, jumps, calls/returns, stop/resume and malicious control cycles.
- 20,000 generated malformed inputs under ASan/UBSan.
- Elite converted from SNA to Z80 gives identical RAM and CPU position after five emulated seconds.
- Existing SNA, keyboard and renderer tests; 600 frames of Elite picture/audio output match the original reference exactly.
- URL filename sanitizer preserves all four extensions; web JavaScript syntax checked; ARM firmware builds.

Hardware validation remains required: cold/warm boot, SD and browser loads, real game tapes (particularly custom loaders), sound/web responsiveness and actual frame rate. Host timing is not a hardware fps measurement.

Specifications: https://worldofspectrum.org/faq/reference/z80format.htm and https://worldofspectrum.net/TZXformat.html .
