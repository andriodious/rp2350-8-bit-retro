# Initial RP2350 8-bit Retro release

A broader Presto emulator platform derived from 8bitkick/rp2350-bbc-micro,
retaining its history and credits and adding the integrated C64 and Nimbus work.

- BBC Model B, BBC Master 128 and Commodore 64 PAL in one combined image.
- Shared web machine selection, keyboard, sound, LEDs and disk-loading controls.
- SD/web disk interfaces, including C64 D64/D81 support.
- Tested C64 SRAM and display-conversion optimisations at 250 MHz.
- Original Nimbus pairing, saved per-machine controls and automatic reconnect.
- Clean Bluetooth shutdown before web machine switches, plus optional diagnostics.
- Machine-neutral sound/keyboard tooltips and new project branding.
- A branded build entry point and combined CI artifact.

Saved Wi-Fi, disk-save locations and Nimbus pairing/profile format are unchanged.
The hostname remains `beeb.local`; use the device's IP address if needed.

This release is based on hardware-tested controller test 5 and the clean Nimbus
build. Project branding and packaging changes receive local build/tests; they do
not constitute new hardware validation or a GitHub CI pass. The original Nimbus
is the tested controller; other gamepads are not claimed supported. For the bundled
C64 Elite disk, decline the fastloader option. See CREDITS.md and the retained
component/asset notices. ROM and game images are downloaded separately at build time.

Local preparation checks: all three release targets built; combined UF2 payloads
and controller storage boundaries verified (10,973 blocks); branded output equals
the combined packer output. Browser tests pass for project naming, machine-neutral
sound/keyboard tooltips, controller controls and the clean diagnostics-off view.
Native persistence, mapping and reconnect sanitizer tests pass. Workflow YAML was
parsed locally, but GitHub Actions has not yet run for this project transition.
