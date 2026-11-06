# Desktop 1581 reference experiment

This is an isolated compatibility experiment, not a Presto firmware feature.
It uses Jondalar/TRX64 at commit
`a86341c8b2add9841dab1a8340fb76d71121e1e3` through its public Rust API.
The normal firmware does not link TRX64. An optional experimental firmware path
is now available; see [hardware build notes](../../docs/c64-1581-experiment.md).
No published release is changed.

## Run

Install Rust/Cargo, clone https://github.com/Jondalar/TRX64, and check out the
commit above. Supply the C64/1541 ROM files in a separate directory. The runner automatically
downloads and verifies the 1581 ROM if it is missing:

- `basic-901226-01.bin` (8192 bytes)
- `kernal-901227-03.bin` (8192 bytes)
- `chargen-901225-01.bin` (4096 bytes)
- `dos1541-325302-01+901229-05.bin` (16384 bytes)
- `dos1581-318045-02.bin` (32768 bytes)

The 1581 reference ROM used locally came from VICE's official mirror:
https://github.com/VICE-Team/svn-mirror/blob/main/vice/data/DRIVES/dos1581-318045-02.bin
SHA-256: `8f689277f36be357f5f105e6e606c35c42e73c581cdeec14b58f31850dfbcfcb`.
ROMs and the game image are not included here. Keep all outputs outside the repo.

`scripts/fetch-1581-rom.py` downloads from a pinned VICE revision and validates
both the 32768-byte size and SHA-256 before installing the file. The cache is
`roms/1581/dos1581-318045-02.bin`, already excluded by `.gitignore`. Valid cached
copies work offline; a mismatched existing file is rejected, never overwritten.
The experimental runner copies the verified ROM to its supplied ROM directory.
Normal firmware builds do not fetch or embed this unused experimental dependency.

To prepare the cache separately:

```sh
python3 scripts/fetch-1581-rom.py
```

For an offline build, set `C64_1581_ROM=/path/to/1581R1-318045-02.u2` or pass
`--source /path/to/1581R1-318045-02.u2` to the fetch script. The same checksum
validation applies. ROM ownership and credits remain with Commodore/its owners.

```sh
python3 experiments/c64-1581/run.py \
  --trx64 /path/to/TRX64 \
  --rom-dir /path/to/roms \
  --image /path/to/Monkey_Island_v1.2.d81 \
  --output /tmp/retro-1581-result --frames 10000
```

The runner first checks the reference commit, ROM sizes and D81 size. It builds
a separate executable with Cargo. The reference is mounted read-only with no
host backing path, so it cannot modify the supplied D81.

The first test boots DOS, uploads six bytes of 6502 code using BASIC `M-W`
commands through the C64 serial bus, sends `U3`, and asserts the program wrote
`$5a` to drive RAM `$0700`. It does not fake the command or directly start the
CPU at the test address.

The second test cold-boots a fresh reference machine and types the same
`LOAD"*",8,1` / `RUN` sequence used by our firmware. It writes a PPM screenshot
and RAM snapshot every 500 frames. A completed frame count is NOT a game
compatibility pass: inspect screenshots for loader progress and the title scene.
Host elapsed time is not an RP2350 performance estimate.

## Existing Presto failure

A native run of our current Frodo integration loads the initial program, then
observes `M-W` uploads to drive RAM followed by `U3`. `Drive::execute_cmd` in
`IEC.cpp` returns ERR_UNIMPLEMENTED for U3. 10,000 frames completed without an
ASan/UBSan report, with no further IEC file opens after the initial `*`.
The D81 is 819200 bytes; all 142 file chains pass structural validation.

## Integration questions

- `C64_rp2350.cpp` runs the C64 CPU per raster line and never runs `TheCPU1541`.
  Merely setting Emul1541Proc does not add a working drive to this loop.
- CIA2 currently talks directly to the 1541 object. A selectable drive bus is
  needed to attach a 1581 and synchronize serial reads/writes.
- Full-drive mode must restore the original KERNAL serial routines; the current
  DOS-level traps bypass serial hardware.
- TRX64's 1581 needs a separate CPU, 8520 CIA, WD1772 controller, IEC wiring,
  8 KiB RAM, and 32 KiB ROM. Its controller advances on register accesses.
- The raw-track buffers, disk image and ROM placement must be accounted for;
  existing C64 static SRAM headroom is only 41220 bytes, not live free heap.
- Keep the normal DOS-level mode for existing titles. Any hardware prototype
  must measure CPU usage, audio underruns, web responsiveness and Nimbus
  reconnection before changing a default.

## Attribution

`reference.rs` is GPL-3.0-or-later. Its test arrangement follows TRX64's
`drive1581_gate.rs`; credit Jondalar/TRX64 and its contributors. TRX64's source
identifies VICE-derived CPU, CIA and disk-controller code. A future port must
retain the relevant notices and assess the licence obligations of the combined
component. This experiment introduces no such linkage into the firmware.

## Result — 5 November 2026

The `M-W` / `U3` probe passed: the uploaded drive program wrote `$5a` at
`$0700`. The supplied Monkey Island v1.2 D81 reached its mountain/title graphic
and opening story text within the 10,000-frame run. Final screenshot inspected.
This establishes loader/intro progress, not full playthrough or save compatibility.
The run completed without a reference-harness assertion failure. TRX64 itself
was a normal optimized build, not a sanitizer build.

The game image was mounted read-only and its SHA-256 remained unchanged.
The initial reference run used TRX64 alone; the subsequent integration below uses Frodo.
No claim about Presto frame rate, sound continuity or RAM fit is made from this
desktop result. The next implementation boundary is a timed IEC drive interface
in the Frodo desktop harness, followed by a separately enabled hardware build.

## Frodo integration and embedded extraction

The isolated `bridge` harness connects Frodo CIA2 serial reads/writes to the
TRX64 drive and advances the drive against C64 CPU instruction cycles and VIC
stolen cycles. It restores the original C64 KERNAL serial routines. All source
patches are applied to a disposable copy of the checksum-verified FRANK sources;
the normal firmware dependency tree is untouched.

```sh
python3 experiments/c64-1581/bridge/run.py \
  --trx64 /path/to/TRX64 --image /path/to/Monkey_Island_v1.2.d81 \
  --output /tmp/frodo-1581-result
```

The full-core bridge passed the uploaded M-W/U3 marker test and reached Monkey
Island's title and opening story in 10,000 frames. C/C++ ran with ASan and UBSan;
the Rust dependency was optimized without sanitizer instrumentation. Leak checks
are disabled because the existing native harness does not tear down the C64.
This establishes introductory loading only, not full game compatibility.

`portable/prepare.py` extracts only the 1581 CPU, CIA, WD controller, disk
mechanism and IEC modules from the pinned checkout, retaining source notices.
It excludes snapshot serialization, replaces lazy lookup-table allocation with
constant read-only tables and removes the unbounded diagnostic PB5 event log.
The generated crate has no external dependencies and builds without `std` for
`thumbv8m.main-none-eabihf`. Its IEC status storage is lock-protected; it is
intended for one emulation session, and must never be called from an interrupt.

Use `--portable` with the bridge runner to test this extracted implementation.
For a separate ARM compilation check:

```sh
python3 experiments/c64-1581/portable/prepare.py \
  --trx64 /path/to/TRX64 --output /tmp/retro-drive1581
rustup target add thumbv8m.main-none-eabihf
cargo build --release --target thumbv8m.main-none-eabihf \
  --manifest-path /tmp/retro-drive1581/Cargo.toml
```

The standalone command produces a Rust library. The subsequent optional firmware
path adds a PSRAM allocator, heap-based array construction, media/reset lifecycle
and CMake integration; see the hardware build notes above. It uses
`thumbv8m.main-none-eabi` to match the SDK ABI. On-device timing remains unmeasured.
