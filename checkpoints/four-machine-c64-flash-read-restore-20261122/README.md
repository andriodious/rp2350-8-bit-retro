# Verified C64 flash-read restoration checkpoint

Hardware verified by the user on 22 November 2026. Reference tag:
`reference/four-machine-c64-flash-read-restore-20261122`.

The exact combined firmware tested on the Presto is retained without rebuilding:
`retro8bit-c64-flash-read-restore-test.uf2` (7,731,712 bytes), C64 image `634e4a505777a51f`.

SHA-256: `2dbdd8b6561539f511c44566a447239294a2c609fa3f073e74ad22c3b43c0a7a`.

## Fix and scope

Saving preferences could return flash reads from quad command `0xeb` / format
`0x492a8` to serial command `0x03` / format `0x1000`. The hardware trace captured
the persistent change during a volume save, followed by settled Monkey Island
D81 slowdown to 86% and distorted sound. The flash wrapper now restores its
entry timing, read command and read format after the final erase/program inside
the existing SRAM callback and interrupt/core lockout, with DSB/ISB ordering.

CPU clock, M1 PSRAM setup, C64/1581 emulation, SID processing, audio clock,
performance options, timer-pool fix, startup watchdog and display DMA ordering
are retained. The C64 runtime diagnostic remains enabled in the tested binary.
The common source fix applies to future rebuilds of other payloads, but this
tested package rebuilt only C64; launcher, Master, BBC Micro and Spectrum were
reused byte-for-byte from the preceding clean clock package. Those machines
were not newly hardware-tested in this sequence.

## Hardware result

- Monkey Island D81, turbo off, sound on: touchscreen volume changed from 84
  to 82. A flash save occurred; before/after and live registers retained quad
  reads, with `flashWrites: 1` and `changed: 0`.
- After the save: 2,796 emulated/rendered frames in 55.919 seconds, approximately
  50.0009 fps. CPU readings were 73% / 36–37%; free RAM stayed at 18,632 bytes.
- The underrun counter rose by 37 in that settled interval, approximately
  0.84 ms of missing audio in total. Loading and intentional menu pauses are
  excluded from this comparison.
- The user confirmed clear audio after the menu/volume change, then confirmed
  repeated settings changes and physical resets. Counts were not specified.
- No recovery/fault was reported. The retained display counters showed two
  missed frames and one late frame across the menu/save, with no resyncs; they
  stayed unchanged in the settled follow-up.

Extracted measurements and user confirmations are in [verification.json](verification.json).

## Host checks and provenance

The old callback reproduced the captured flash downgrade. The fixed callback
passed 246 ASan/UBSan host cases, including repeated saves, both entry read
modes, partial pages, lockout/IRQ preservation and untouched M1 registers.
Runtime collector, menu, audio and startup-watchdog checks passed. The final
ELF audit verified SRAM/ROM execution and barrier/store ordering. Firmware
payload mapping and protected binary checks passed.

The local archive `artifacts/four-machine-c64-flash-read-restore-verified-20261122`
beside this checkout contains the exact tested UF2, original build evidence,
ELF/map, host logs, source patch and checksums. Source started at
`60c87fa9850314bf41db86ac7977e905874a6fb0`; this checkpoint adds the matching
fix/diagnostics/tests and these verification records. Documentation added after
testing does not change the retained binary. A fresh build is a separate test
artifact and is not automatically hardware verified.

v0.9.0 and earlier checkpoints remain unchanged. No ROMs, game images or firmware
binaries are committed. This checkpoint does not publish a new release.
