## Four classic machines on one Presto

v0.5.0 brings ZX Spectrum 48K alongside BBC Master 128, BBC Model B and Commodore 64.

- Spectrum at 50 fps with built-in Elite, beeper sound, SNA/Z80 snapshots and TAP/TZX loading from SD or web upload.
- Spectrum web keyboard with grey keys, black case and rainbow stripe; Kempston arrow/Fire controls. Reset returns to BASIC; Restart Elite reloads the snapshot.
- BOOT SD chooser: with an enabled, connected joystick and compatible SD titles, use up/down to browse and Nimbus A or mapped Fire to load. BOOT then cycles to status and back to emulation. Without a usable controller/title list, BOOT toggles status directly.
- Emulation pauses and audio mutes while the chooser/status screen is open. Web access remains active. Flicker-free operation and selection verified on all four machines.
- Saved per-machine sound, volume, LED/brightness settings and controller mappings; improved Spectrum/C64 keyboard styling.
- Existing D81 gameplay/audio improvements and Wi-Fi/original Nimbus fixes retained.

## Install

Download **rp2350-8-bit-retro-presto.uf2**. Hold BOOT, tap RESET, then release BOOT to expose the Presto USB drive. Copy the UF2 to it; power-cycle if necessary. Open **http://8bitretro.local/** or the device IP to choose a machine, configure controls and load media.

Existing Wi-Fi settings, machine choice and saves are retained. New users can follow the [Wi-Fi setup guide](https://github.com/andriodious/rp2350-8-bit-retro/blob/retro-platform/docs/presto/wifi.md).

## Validation and identification

The attached UF2 is the exact hardware-verified checkpoint `reference/boot-sd-chooser-20261111` (97311b1), not a replacement rebuild. Its source label still reads `reference/boot-status-20261111-dirty`; the manifest and image IDs identify the tested binary:

| Master | Beeb | C64 | Spectrum |
|---|---|---|---|
| 2bc54b12478ef597 | f53f40e25c9256bf | f2dd8f527fa0896e | a5a067cf2350b2aa |

A separate clean checkout from GitHub built all four machines and passed firmware integrity, controller, display, deterministic Spectrum Elite/audio and snapshot/tape tests. Spectrum source matches the pinned GitHub fork, and Elite images are downloaded during build preparation, not stored in Git. The release source adds documentation and CI updates to the tested checkpoint.

Spectrum is 48K only; not every TZX encoding or game is supported. Original Nimbus is tested, not Nimbus+. C64 D81 scene loading can still interrupt audio; for the bundled C64 Elite choose N at its fastloader prompt. Previous release downloads and reference tags remain available.

SHA256SUMS covers the downloadable firmware, source archive and manifest. No plugin or replacement save-disc payload is included.
