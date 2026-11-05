# C64 cartridge and P00 support

Select C64 and choose a `.crt` or `.p00` file from the SD card or web upload.
ZIP uploads use the same supported-file selection as disks and T64 archives.
Added after v0.3.0. Hardware testing confirmed COMMANDO.P00 and
Monkey_Island_V1.2.crt on Pimoroni Presto. Other titles and cartridge hardware
still require compatibility testing.

## P00 programs

The loader validates the `C64File` header, PRG load address and 64 KiB RAM bounds.
It retains a private copy in the existing mounted-media PSRAM area. After a reset
and BASIC startup delay, a single runnable BASIC program loads and runs, just as
with a single-entry T64. A BASIC SYS stub is supported. Other machine code loads
without guessing an entry address. The T64 / P00 panel provides Load and Load and
run; Run is available only for structurally valid BASIC at $0801. Reset cancels a
pending load; Boot repeats it. Disk/tape follow-on loads are not provided by P00.

## CRT cartridges

Supported CRT v1.0 hardware types:

- Type 0: standard 8 KiB or 16 KiB ROM (8 KiB CHIP blocks, or one 16 KiB block).
- Type 32: EasyFlash, up to 64 banks of 8 KiB ROML plus 8 KiB ROMH.

The upload limit remains 1 MiB including headers. EasyFlash bank 0 must include
both boot ROM halves. Other cartridge hardware, raw BIN images, generic Ultimax,
RAM CHIP packets, flash programming and persistent EasyFlash saves are not
supported. Unsupported or truncated files are rejected before the running
cartridge is replaced. Allocation failures also preserve the current cartridge.

Cartridge insertion resets the C64 without typing disk LOAD commands. Boot and
Reset restart the cartridge. Eject cartridge removes it and resets to BASIC.
Loading a disk, T64 or P00 automatically detaches the cartridge. Cartridge state
is session-only; switching machines or powering off removes it.

The bounded loader lives in `src/machines/c64/crt.cpp`. Its ROM mapping follows
FRANK/Frodo (Christian Bauer and rh1tech/frank-c64; GPL-2.0-or-later). It allocates
checked PSRAM buffers, never programs RP2350 flash and does not use the upstream
file loader. EasyFlash owns 1 MiB of ROM storage; replacing it temporarily needs
storage for both cartridges. The existing allocator releases the old image after
a successful swap. The core patch also initializes `border_40_col` consistently
with its zeroed control register, fixing an uninitialized read before BASIC starts.

## Validation

`sh tests/c64/native/build.sh media` runs generated, redistributable fixtures
through the actual C64 core under address/undefined-behaviour sanitizers: P00 BASIC
execution, EasyFlash reset-vector execution, bank switching, standard cartridges,
malformed/truncated packets, allocation failures and repeated insertion/ejection.
The existing T64 parser/API/browser tests remain applicable; browser tests now
cover P00/CRT upload and cartridge ejection as well.

Optional local samples (not included in Git or firmware):

```
ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 /tmp/c64host/test /path/COMMANDO.P00 /path/Monkey_Island_V1.2.crt
```

The supplied COMMANDO.P00 is a 9,549-byte runnable program at $0801.
Monkey_Island_V1.2.crt is a 722,368-byte EasyFlash image containing 88 CHIP packets.
Native execution is a smoke test, not proof of full game compatibility. The
harness does not tear down the entire emulator, so leak detection is disabled.

On hardware, test both files from web and SD. Check sound, Nimbus controls,
cartridge Boot/Reset/Eject, then cartridge-to-P00-to-disk transitions and machine
switching. Verify repeated loads remain stable and malformed uploads leave the
existing cartridge running. EasyFlash saves are not supported. Keep v0.3.0 as a
known-good fallback until these checks succeed.

## Hardware result

On 5 November 2026, the owner confirmed that both supplied samples worked on
Presto with the cartridge/P00 test firmware. This confirms those two examples;
it does not establish compatibility with all CRT types or EasyFlash saving.
