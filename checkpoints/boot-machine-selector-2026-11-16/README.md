# Verified four-machine BOOT machine selector

Hardware tests passed on 16 November 2026, confirmed by the user. BOOT cycles
through file chooser, machine selector, status and back to the emulator.
Keyboard Up/Down and Return and Nimbus Up/Down and A/Fire select machines.
Selecting the current machine resumes cleanly. Keyboard and Nimbus reconnect
after switching machines.

The tested combined UF2 checksum and component identities are in verification.json.
The firmware artifact is retained locally and is not committed. Automated tests
covered all four menu/render configurations, keyboard and joystick selection,
bounds, held-input suppression, error handling and key ownership. The combined
four-machine build succeeded.

Reference tag: reference/four-machine-boot-selector-20261116.
Build recipe: scripts/build-classic-keyboard-four.sh with an isolated output folder.
This checkpoint preserves the previous checkpoints and published v0.6.0 release.
