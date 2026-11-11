CHECKPOINT: Hardware verified by user on 2026-11-11.

Experimental four-machine Presto firmware

Includes BBC Master, BBC Micro, C64 and Spectrum 48K. Flash
presto-four-machine-experimental.uf2, then fully power cycle.
The clean launcher starts Master on cold boot/physical reset. Select
ZX Spectrum from the web machine selector. Spectrum status should show
image fe895e58b0e59d33.

Spectrum includes Elite, the grey-key web keyboard, SD loading and
SNA/Z80 snapshots plus TAP/TZX playback from the tested media build.
Not all TZX encodings are supported; unsupported blocks report errors.
The C64 D81 optimisations, starting screen and Wi-Fi/Nimbus fixes are retained.

Existing machine slots and persistent-storage locations remain unchanged.
Spectrum occupies the unused slot at 0xc08000..0xdfffff. The UF2 does not
include application or save-storage payloads. Nimbus uses a separate Spectrum
mapping extension; old firmware can still read the original three mappings.
An old firmware settings write may discard the Spectrum mapping extension.

First hardware checks: Master to Spectrum (Elite, sound, keyboard, TAP/TZX,
SD), then C64 D81 and back to Master/Beeb. Confirm web responsiveness and
warm/cold resets. User confirmed all checks verified on 2026-11-11. This is the new experimental checkpoint.

Restore firmware is retained at:
../combined-clean-c3c3c2b/presto-combined-clean.uf2
The verified standalone Spectrum and clean BBC/C64 references are unchanged.

Source and ELF symbols accompany this package. Build entry point:
scripts/build-experimental-four.sh. Toolchain used: Arm GCC 14.2,
Pico SDK 2.2.0 and Rust 1.99.1. TRX64 pinned revision is in manifest.json.

GitHub stores checkpoint metadata only; the UF2 and source/symbol archives remain local.
Elite is fetched at configure time rather than committed to Git.
