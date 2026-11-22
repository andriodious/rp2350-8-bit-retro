# C64 menu/save runtime diagnostic trial

The collector originally added observation only to checkpoint
`60c87fa9850314bf41db86ac7977e905874a6fb0`. This follow-up trial retains it alongside
the flash read restoration described in `c64-flash-read-restore.md`. Neither
changes the C64 or 1581 emulators, SID synthesis, audio clock, volume curve,
display restart sequence, network timer pool or watchdog recovery.

Enable with `-DC64_RUNTIME_DIAG=ON` in addition to the existing four-machine
release options. Only the C64 target includes the collector. Normal builds use
empty inline hooks. Other payloads in this test package are copied unchanged
from the completed clean checkpoint build.

## Hardware test

1. Flash the combined diagnostic UF2 and power cycle. Confirm the C64 image ID
   in its accompanying artifact README.
2. Load Monkey Island D81 and wait until the scene has settled. Save
   `/api/status` and `/api/c64-runtime` from that Presto.
3. Open the touchscreen menu, leave it open for several seconds, change volume
   once, and exit. Wait a few seconds for any deferred preference save.
4. Without resetting, capture both endpoints again. Report whether sound and
   speed changed. The ring holds the most recent eight transitions; avoid
   repeated menu openings until those captures are saved.

## Capture semantics

`/api/c64-runtime` is read-only, as is the compact `c64_runtime` fragment in
`/api/status`. The full reply labels each register array and event ID. It holds
an immutable boot baseline, the last eight transitions in chronological order,
and current registers. The events bracket menu entry/exit and sector saves.
Flash captures execute in SRAM inside the existing interrupt/core lockout,
before erase and after M0 timing, read-command and read-format restoration. Capture performs no
allocation, logging, ROM calls or peripheral writes.

Snapshots record both QMI banks, XIP control, five BOOTRAM words, system clock
and PLL settings, plus the cached ROM flash-device-information value. A null
devinfo value means its pointer was unavailable or failed validation. The
reported clock frequency is the SDK's configured value, not a measurement.
Live display counters and the attempted flash-write count are read during
JSON serialization, outside the flash callback. The display counters are not
atomic across an interrupt and should be compared over a settled interval.

The compact `changed` bitmask compares live values with boot: bit 0 QMI M0,
bit 1 QMI M1, bit 2 XIP control, bit 3 BOOTRAM, bit 4 clocks/PLL, bit 5 devinfo.
A difference is evidence to inspect, not itself proof of a fault. Initial ROM
flash setup can legitimately change BOOTRAM after boot.

Neither opening the diagnostic endpoint nor recording transitions resets the
emulator, drive, audio or display counters. The event ring is volatile and is
lost on reset; existing terminal-caller/watchdog recovery remains enabled.
The diagnostic has a small SRAM/code cost, so compare its before/after results
within the same run before drawing performance conclusions.
