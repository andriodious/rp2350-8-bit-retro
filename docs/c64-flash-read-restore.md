# Preserve the flash read mode after saving settings

Hardware diagnostic image `af26b56bf4ad537e` captured a persistent M0 read-mode
change during a volume preference save at flash offset `0xf00000`. Immediately
after erase/program, RFMT changed from `0x492a8` to `0x1000`, and RCMD from `0xeb`
to `0x03`. The change survived closing the menu. Timing, M1 PSRAM settings,
XIP control and CPU clock registers stayed unchanged. The matching Monkey Island
D81 status then reported 86% speed, compared with 99% before the save.

The original format sends a serial command and uses quad address/data transfers.
The replacement format uses serial transfers throughout. The retained flash
clock divider does not compensate for this loss of transfer width. This can
increase stalls in code and data still fetched from flash, without disabling
the emulator's SRAM placement or drive-loop optimisations.

The existing flash wrapper restored only M0 timing after the SDK operations.
The correction also saves and restores M0 RCMD and RFMT. It runs after the final
erase/program operation, inside the existing SRAM callback, with interrupts
disabled and the other core parked. DSB orders outstanding accesses before
restoring timing, command and format; DSB/ISB complete the restoration before
returning to flash-resident code. It restores the actual entry values rather
than imposing a board-specific command. The captured mode uses an explicit
command on each read, so no continuous-read or QPI entry sequence is needed.

M1, device information, XIP control, BOOTRAM and CPU clocks are not changed.
The SDK already handles cache cleaning/invalidation around erase/program; the
fix does not add a broad cache flush or a ROM mode setter that could alter PSRAM.

The combined trial rebuilds only C64, retaining diagnostics so the before/after
events can prove that the settings save no longer changes the read mode. The
other three machine components and launcher are reused unchanged. Although the
wrapper is common source, this package does not rebuild the other payloads.
All existing stability fixes and C64 optimisation options remain enabled.

Hardware acceptance: repeat a settled D81 scene, enter the touchscreen menu,
change volume, exit, then capture `/api/status` and `/api/c64-runtime`. A save
must occur (`flashWrites` increases), while FLASH_END and live M0 settings must
match FLASH_BEGIN. Check restored speed, sound and web responsiveness, repeat
saves, and physical resets. Host tests and register restoration alone do not
establish that every audible distortion has been resolved.

## Verified checkpoint

The user confirmed the fix on hardware, including clear sound after saving,
repeated settings changes and physical resets. The settled D81 interval held
50.0009 fps with unchanged quad-read settings. See
[the retained checkpoint](../checkpoints/four-machine-c64-flash-read-restore-20261122/README.md)
for exact firmware identity, test scope and measurements.
