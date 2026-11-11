# Verified four-machine experimental checkpoint

Hardware verified by the user on 11 November 2026: BBC Master, BBC Micro,
C64 and Spectrum 48K. The image identities and firmware checksum are retained in
`checkpoints/four-machine-2026-11-11/`.

Build with `scripts/build-experimental-four.sh`. This checkpoint preserves the
C64 D81 optimisations, web starting screen and Wi-Fi/Nimbus fixes, and adds
Spectrum Elite, 50 fps rendering, keyboard, SD, SNA/Z80 and TAP/TZX support.
Existing machine slots and persistent settings locations remain unchanged.

Validation: combined UF2 component/partition integrity; Nimbus store compatibility
under ASAN/UBSAN; Spectrum media parsing and ROM loading tests; 600-frame
picture/audio equivalence; web switch races; C64 D81 SRAM placement and audio;
Wi-Fi startup gates. The user subsequently confirmed all hardware checks.

The clean BBC/C64 and standalone Spectrum references remain available. This is
an experimental checkpoint, not a replacement stable release.

The verified UF2 is retained locally, not committed (it embeds Elite).
CMake downloads Elite using `scripts/fetch-spectrum.py` and validates its SHA-256.
For offline builds set `SPECTRUM_ELITE_FILE` to the pinned snapshot.
The emulator fork is https://github.com/andriodious/chips, branch
`presto-spectrum-48k`; `src/cores/chips/UPSTREAM` pins the vendored revision.
