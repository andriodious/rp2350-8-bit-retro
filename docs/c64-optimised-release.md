# Optimised C64 defaults and hardware results

The updated PR enables these four options in fresh C64 builds:

| Option | Default | Purpose |
| --- | --- | --- |
| C64_RAM_VIC | ON | Execute VIC graphics code from internal SRAM |
| C64_RAM_CPU | ON | Execute the main 6510 interpreter from internal SRAM |
| C64_RENDER_OVERLAP | ON | Convert the previous indexed frame while emulating the next |
| C64_PACKED_PIXELS | ON | Read four indexed pixels with one word load |
| C64_PROFILE | OFF | Optional timing collection and web performance panel |
| WEBDIAG | OFF | Optional diagnostic trace/test-speaker UI |

Clock remains 250 MHz; Master remains the power-on default. Sound and ambient
lighting remain configurable through the web interface. A normal combined build
uses the optimisations without extra flags. Existing CMake build directories
retain cached options: use a fresh folder when upgrading.

## Measurements

The contributor captured repeated five-second windows through the web interface
on the same Elite title scene at each stage. These are instrumented firmware
results, not measurements of the final non-profiling release binary.

| Stage | Normal emulated fps | Turbo emulated fps |
| --- | ---: | ---: |
| Original profiling baseline | 36.90 | 41.80 |
| VIC SRAM | 41.92 | 48.03 |
| Render overlap | 44.96 | 50.93 |
| Packed pixel reads | 45.68 | 51.59 |
| CPU interpreter SRAM | 50.00 | 62.48 |

Final normal-mode capture: 15 windows, 75.20 seconds, 50 fps throughout; average
emulation time 13.57 ms/frame with 6.17 ms deliberate pacing. Turbo capture:
16 windows, 80.16 seconds. Turbo still converts alternate frames; 62.48 emulated
fps is approximately 31.25 converted display frames/sec.

The contributor reported normal operation and much better sound and accepted the
hardware testing. A later status snapshot with Ghosts 'n Goblins loaded reported
43,208 bytes free, Wi-Fi up and 50 fps in the latest window. Sound was disabled in
that particular snapshot; audible improvement was reported separately. This is
not a measured long-duration memory-leak test or a guarantee for every game.

## Memory and correctness checks

Overlap adds a 104,448-byte indexed PSRAM framebuffer plus allocator overhead.
The CPU SRAM step adds 24,576 bytes static SRAM including link overhead. The clean
non-profiling local build leaves 48,016 bytes to the ordinary heap limit before
runtime allocations; this is distinct from the device's reported live free RAM.

Checks performed during development:

- Matching hashes for all 11,000 native BASIC/Elite test frames with overlap OFF/ON,
  plus verification that the converter's input remains unchanged during the next
  emulated frame. Native testing uses sanitizers but cannot reproduce ARM timing.
- All 65,536 four-pixel palette combinations and complete-frame/padding comparison
  pass for packed versus scalar conversion under sanitizers.
- Linked CPU/VIC placement verified; disabled variants checked against baseline.
- Browser profile collection/download, counter handling and dependency checksum
  tests pass. FRANK history/licences and pinned revision are retained.
- Fresh clean combined build succeeds with all four optimisations ON and both
  profiling/WEBDIAG OFF. The contributor also rebuilt on a Raspberry Pi with these
  settings. That UF2's 10,153 payload blocks match all four inputs without overlap.

The newly rebuilt clean binary still merits a brief boot/switch/audio check before
publishing it as a release artifact. Profiling hardware results apply to the same
optimised algorithms but do not replace validation of every produced binary.

## Remaining limitations

Custom 1541 fastloaders are unsupported; answer N to Elite's fastloader prompt.
C64 disk writes are session-only. The LCD scanout buffer remains single-buffered;
overlap does not guarantee tear-free output. Audio output quality and amplifier
noise remain hardware-dependent. The 250 MHz clock remains an overclock, unchanged
by this work. Historical experiment documents record intermediate states; this
page and c64-pr-build.md describe the final defaults.
