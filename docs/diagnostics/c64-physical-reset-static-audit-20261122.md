# C64 physical RESET: static audit, 22 November 2026

## Result and scope

The old intermittent physical RESET failure is **not reproduced or conclusively
explained**. This audit found a specific early-startup coverage gap, a separate
display-DMA erratum concern, and an external PSRAM chip-select lead requiring
board-revision verification. It found no invalid initial reset vector, static
startup-memory overlap, or launcher machine-selection error in the tested paths.

This is separate from the captured C64 runtime panic caused by lwIP timer-pool
exhaustion. The latter has a decoded hardware record and a host reproduction;
neither establishes the cause of the historical physical RESET blank screen.

The running Presto was not contacted, reset, flashed or switched. Its ongoing
timer-pool test remains undisturbed. No firmware source, release, tag or checkpoint
was changed by this audit. Only this report and a host launcher test were added.

## Host execution checks

Run `python3 tests/startup/launcher_selection_test.py` from the repository root.
The test executes the actual current `launcher.c` and `system.h`, using the pinned
SDK partition field definitions and the real partition JSON. ROM calls and reset
registers are simulated. The final diagnostic reporting loop is replaced with a
test return; it does not test USB initialization or LED output.

Both normal launcher and diagnostic-report configurations passed, with address
and undefined-behaviour sanitizers:

```text
launcher report=0: 49156 reset/settings/fallback cases and 100000 malformed records passed
launcher report=1: 49156 reset/settings/fallback cases and 100000 malformed records passed
```

Coverage includes all four saved machines; simulated reset-reason values and
armed-watchdog markers; every combination of missing partition lookups and failed
image handoffs; complete failure to USB/report fallback; correct partition base
and length; preservation of scratch registers 1–7; bounded saved-machine string
validation; and missing/damaged/unterminated/unsupported settings.

The saved C64 choice remains C64 on an ordinary reset in this code. An armed C64
watchdog failure selects Master and preserves the failure record. Failure of all
four returning image attempts reaches the deliberate fallback. A ROM call which
does not return is outside the host model.

The older `tests/c64/launcher_fallback_test.py` and
`tests/startup/launcher_report_test.py` are historical tests, not current evidence:
they retain pre-Spectrum slots/arrays and, in the former, the previous cold-boot
Master expectation. They were not used as passing checks. The new test covers
the current four-machine implementation without altering those historical files.

## Retained binary inspection

Read-only inspection used the exact `build-network-timers` C64 and Master ELFs.
C64 image is `d6732ae6816431fa`; Master is `3a08f54f167aeb29`. These are test images,
not changes to v0.8.0.

| Item | C64 | Master |
|---|---|---|
| Initial stack pointer | `0x20080000` | `0x20082000` |
| Reset vector, Thumb bit set | `0x1000015d` | `0x1000015d` |
| Initialized-data destination | `[0x20000130,0x2000cd10)` | `[0x20000150,0x20025cc4)` |
| BSS clear range | `[0x2000cd10,0x20074f84)` | `[0x20025cc4,0x200674a8)` |
| Core 0 stack | `[0x2007c000,0x20080000)` | `[0x20081800,0x20082000)` |
| Core 1 stack | `[0x20080800,0x20081000)` | `[0x20080800,0x20081000)` |

Actual startup copy tables agree with the linker symbols; the ranges are aligned
and do not overlap the reserved stacks. Both contain ordinary SDK startup,
including early and post-clock peripheral-reset routines. This does not measure
runtime stack high-water marks or rule out arbitrary memory corruption.

The C64 `psram_setup` and its `direct_command` callee are in SRAM (`0x20006564`
and `0x20006548`). Their inspected disassembly calls no flash-resident routine
while QMI direct mode is active; the reset delay is an inline loop. The recent
caller diagnostic has `STARTUP_DIAG=OFF` and no older startup-marker functions.

## Physical RESET is not an emulator reset or a core-1 shutdown

The published Presto schematic connects RESET directly to the RP2350 RUN pin.
The RP2350 datasheet describes RUN as a chip reset which restarts both processors
in the boot ROM, with core 1 waiting there. It resets peripheral hardware too;
the SDK subsequently also resets DMA, PWM and PIO during normal initialization.
The watchdog's `PSM_WDSEL` choices do not define the physical RUN reset path.

Consequently, the simple explanation that a normal core-1 software lockout
survives an actual RUN reset is not supported by the documented reset behavior.
This does not rule out a hardware erratum or an external memory/bus problem.

The web emulator reset instead sets `reset_pending`, and `c64_reset()` resets the
emulated C64 components. It does not perform the same hardware boot sequence.
Software machine switching uses the saved choice and watchdog reboot path.

Sources: [Presto schematic, sheets 1–2](https://cdn.shopify.com/s/files/1/0174/1800/files/pico_presto_schematic.pdf?v=1734528559),
[RP2350 datasheet, sections 7.3.1, 7.4.1 and 7.5.2](https://datasheets.raspberrypi.com/rp2350/rp2350-datasheet.pdf).

## Concrete early-startup gap

In `src/machines/c64/main.cpp`, C64 disables the watchdog, initializes stdio and
the board, and performs PSRAM and emulator-memory initialization before enabling
the normal watchdog. Display and network startup come afterward.

`src/boards/presto/presto_psram.c` waits indefinitely for QMI BUSY to clear both
on entering direct mode and after each direct command. The uncached PSRAM test
that follows also assumes memory transactions complete. If hardware stalls in
this interval, there is no normal C64 watchdog coverage to recover it, and no
display or web interface has been initialized. This is a code path consistent
with a dark screen; it is not evidence that the historical failures reached it.
An absent normal USB firmware drive also does not localize startup: that drive
belongs to ROM BOOTSEL, not ordinary application operation.

Both Master and C64 slow the flash before choosing 1.25 V / 250 MHz and use the
same PSRAM setup: leave QPI, reset-enable, reset, delay, then re-enter QPI. Core 1
is not launched yet. The imported FRANK board-specific main is not used in the
Presto build. No active C64-only reset-vector or flash-mode setup was found.
Shared setup means this gap alone does not explain the machine difference.

## External PSRAM chip-select lead: not confirmed on these boards

The RP2350 datasheet says Bank 0 GPIOs return to high impedance with pull-down
on RUN reset, and requires an external pull-up when a Bank 0 GPIO is used as
the second QSPI chip select. RUN also resets the pad isolation latches; the
previous driven-high state cannot be assumed to survive.

The published Presto schematic shows PSRAM CS on GPIO47, with PSRAM sharing data
and clock with flash and remaining powered from 3V3. No pull-up on PSRAM_CS is
shown on that schematic; a pull-up is shown on the primary flash chip select.
If the fitted board also lacks the needed pull-up, the PSRAM may be selected
during reset and could interfere with flash startup before `main()` or our
PSRAM reset code can execute. That is an inference requiring measurement, not
a demonstrated failure.

**Revision limitation:** this schematic does not show the later three-pin DEBUG
socket present on the user's board. It has no usable revision identifier. The
current [Pimoroni product page](https://shop.pimoroni.com/products/presto) notes
that the debug connector was added after June 2025. Do not conclude that the
user's newer PCB is missing a resistor from this drawing alone, and do not
prescribe a board modification on this evidence.

The chip documentation also explicitly distinguishes RP2350 reset from reset
of attached QSPI devices. ROM has an XIP-exit sequence, with CS1 handling dependent
on FLASH_DEVINFO. The application does not configure FLASH_DEVINFO; the board's
OTP provisioning was not read. The application PSRAM recovery commands therefore
cannot guarantee the preceding ROM flash-boot interval was safe.

Sources: [RP2350 datasheet, sections 9.3, 9.7, 5.1 and 5.4.8.7](https://datasheets.raspberrypi.com/rp2350/rp2350-datasheet.pdf),
[SDK 2.2.0 ROM flash API](https://github.com/raspberrypi/pico-sdk/blob/2.2.0/src/rp2_common/pico_bootrom/include/pico/bootrom.h),
and the Presto schematic linked above. This lead does not yet account for Master
passing the earlier reset tests.

## Separate display-DMA concern

`src/boards/presto/presto_lcd.c`, in the frame interrupt resynchronization path,
aborts the two chained display DMA channels without first clearing their enable
bits. The pinned SDK's `hardware/dma.h` documents that RP2350-E5 requires clearing
the enable bit of the aborted channel and chained channels before aborting to
prevent retriggering. The existing audio diagnostic stop path already follows
that ordering.

This is a concrete mismatch worth reviewing separately. The path is shared by
Master and C64 and is reached during display resynchronization, not as a
physical RESET handler. It does not prove the old reset failure was a DMA hang.
C64's larger display row transfers and different audio DMA traffic may affect
timing, but their contribution has not been measured.

Source: [SDK 2.2.0 DMA abort documentation](https://github.com/raspberrypi/pico-sdk/blob/2.2.0/src/rp2_common/hardware_dma/include/hardware/dma.h).

## What remains necessary to locate the old failure

Finish the independent timer-pool stability test first. For a later reset test,
use one fixed binary and one saved destination, record the first RESET separately
from subsequent presses, and compare C64 and Master under those controlled
conditions. Earlier diagnostic versus clean comparisons changed instrumentation,
layout and sometimes the destination machine simultaneously.

If failure recurs, read-only SWD inspection can establish whether execution is
in ROM, CRT, USB, PSRAM setup or later. Verification of the exact PCB revision
and PSRAM_CS level during RESET would test the external-memory lead. New LED or
backlight probes alone would again change timing and cannot observe ROM entry.

Seven unchanged green LEDs are retained display state, not evidence of a newly
reached checkpoint. The earlier guards finding no corruption and the new host
tests passing narrow the investigation; neither proves every hardware reset is
safe. No speculative reset fix or new firmware build is supplied by this audit.
