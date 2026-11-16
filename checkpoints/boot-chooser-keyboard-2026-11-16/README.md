# BOOT chooser Bluetooth keyboard verification

Hardware verified by the user on 16 November 2026 on BBC Master, BBC Micro,
C64 and Spectrum. Keyboard-only Up/Down and Return selection, Nimbus selection
and clean menu resume passed: “works perfectly”.

The chooser accepts raw cursor keys and Return/keypad Enter independently of
saved emulator mappings. Captured keys are suppressed until released after
resuming. Existing joystick selection and BOOT cycling are preserved.

Automated checks passed for all four chooser configurations under ASan/UBSan,
controller capture and key ownership, HID mappings, mapping commands and Wi-Fi
reconnection. The combined firmware build succeeded. See verification.json for
the tested UF2 checksum and component identities. The UF2 remains a local test
artifact; it is not committed to GitHub.

Build using scripts/build-classic-keyboard-four.sh with an isolated output folder.
The published v0.6.0 release and previous reference checkpoints remain unchanged.
