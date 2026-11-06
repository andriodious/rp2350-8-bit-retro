# Warm-reset startup diagnostics

Experimental, opt-in `STARTUP_DIAG=ON` for **both the launcher and all machine
images**. Keep the 5 ms D81 baseline options. Rear ambient colours are replaced
by a coloured count of the latest startup marker for the whole session.
No watchdog policy, scratch registers, flash settings, D81 scheduling or radio
retry policy is changed. No automatic reset is added.

## Diagnostic v2: early interval

V1 reproduced the warm-reset black screen with one green LED; cold power-on
restored operation. V2 subdivides the launcher-to-board interval without
changing the baseline reset/clock/drive/radio configuration.

| Blue LEDs | Launcher progress |
|---|---|
| 1 | Entered launcher, before partition-table loading |
| 2 | Partition-table loading returned; before machine selection |
| 3 | Entered selected partition lookup |
| 4 | Partition lookup returned |
| 5 | About to call ROM firmware handover |
| 6 | ROM handover returned rather than entering the firmware |
| 7 | All candidates exhausted; entering USB bootloader |

| Amber LEDs | Machine progress |
|---|---|
| 1 | Entered machine main, before early setup |
| 2 | Flash timing setup returned, before voltage/clock change |
| 3 | Clock setup returned, before remaining early setup and stdio |
| 4 | USB/stdio initialisation returned, before board setup |

Five blue LEDs followed by no amber marker includes the ROM handover **and**
the selected firmware's C runtime before main; it does not distinguish those.
Amber one includes Master failure-record capture or Model B boot guard before
flash timing. Colours/counts report progress, not confirmed fault locations.

| Green LEDs | Later marker reached |
|---|---|
| 1 | Used only by diagnostic v1; v2 launcher markers are blue |
| 2 | Machine board initialisation entered (after clock/stdio setup) |
| 3 | PSRAM initialisation entered, before QMI setup and memory probe |
| 4 | Display initialisation entered |
| 5 | Display initialisation returned to its final backlight-enable point |
| 6 | Network startup entered, before radio initialisation |
| 7 | Frontend reached 100 frames |

These mark entry/progress, not proof that a subsystem is healthy. A failed
PSRAM probe can return zero rather than hang. Seven LEDs do not prove DHCP or
HTTP success. Network startup can be delayed, so count six after seven is
possible if startup ordering differs. Both cores can reach display/frontend
markers; report the observed count rather than assuming strictly ordered stages.

Before ambient initialisation, the marker briefly borrows PIO0 and releases its
state machine/program after sending seven pixels. After ambient initialisation,
it uses that driver's state machine; normal ambient updates are suppressed.
Each marker allows 600 microseconds for output/latch, so instrumentation can
alter startup timing and may mask a timing-sensitive failure. There are no
intentional multi-second pauses. Normal builds compile all marker calls away.

Test from a cold power-on first: note whether all seven LEDs appear. Run D81
until the mountain screen and leave it running, then press physical Reset.
Record the rear LEDs on video if possible: report the final colour and count when
the screen stays black and whether the web interface returns after 30 seconds.
If no new count appears after Reset, the previous LED state may simply be
latched; that does not prove that the launcher executed. A power cycle erases
this visible history, so note the count before removing power.

The baseline is commit a306731. Warm reset failure after longer D81 execution
is confirmed by the user; early-loading resets can work, and a full power cycle
reliably restores operation. The earlier post-flash-only interpretation was
incomplete. These diagnostics locate progress; they do not fix the cause.

Validation before hardware testing: sanitizer-backed checks of the real early
LED sender cover LED count, program/state-machine release, unavailable-resource
paths and handoff to the ambient driver. Existing polling-budget, Wi-Fi gate,
Nimbus reconnect, HTTP lifetime, profile and network JSON tests passed.

Enable diagnostics explicitly on cached builds as well as fresh ones:

```sh
for machine in master beeb c64; do
  cmake -S . -B build-1581-test/$machine -DSTARTUP_DIAG=ON
done
cmake -S src/launcher -B build-1581-test/launcher -DSTARTUP_DIAG=ON
scripts/build-retro.sh build-1581-test
```

To return to normal ambient behaviour, set STARTUP_DIAG=OFF in all four build
directories and rebuild, or flash the retained 5 ms baseline firmware.

Firmware validation: all four images compiled; combined UF2 payload/partition checks passed (11,774 blocks). Marker symbols and ambient-driver handoff were verified in all images. C64 idle helper remains 822 bytes in SRAM, general dispatcher in flash; static heap headroom is 32,564 bytes. Hardware marker visibility and reset reproduction remain untested.

V2 changes only marker granularity/colours and machine-entry probes. The 5 ms polling and D81/Wi-Fi/Nimbus logic remain unchanged. Hardware validation of v2 is pending.

V2 validation: all four images compiled; combined UF2 checks passed (11,776 blocks). LED colour/count and handoff tests, 5 ms timing and Wi-Fi gate tests passed. SRAM helper remains 822 bytes; static heap headroom 32,564 bytes. This is diagnostic instrumentation, not a confirmed reset fix.

## Diagnostic v3: partition loading and flash return

Hardware v2 result: after a prolonged run, Reset left black display/all green;
a second Reset left one blue LED. Power cycling restored operation. The first
all-green observation may have been latched; the one-blue observation narrows
progress to the interval around partition loading, not a proven fault inside it.

The SDK 2.2.0 wrapper uses a non-blocking SHA lock. An unavailable lock returns
an error; it does not spin. The 4096-byte aligned work area exceeds the documented
3264-byte requirement and is separate from the launcher scratch-Y stack.
Raspberry Pi's public A4 ROM source, inspected at
`c6cdb1711f32c3e34faaebd58618a6d096dbd52e`, initializes a scan context, then skips
rescanning if a resident table is already loaded and force_reload is false.
Otherwise it scans using the existing XIP setup. This source is reference evidence;
the board's actual ROM revision has not yet been established.
Source: https://github.com/raspberrypi/pico-bootrom-rp2350/blob/c6cdb1711f32c3e34faaebd58618a6d096dbd52e/src/main/arm/varm_flash_boot.c

V3 preserves all v2 blue/amber/green markers. Between one and two blue, the
launcher now runs a diagnostic wrapper from SRAM, with its own SRAM-only LED
sender and literals. PIO is configured before entering this path, and released
on successful return. No flash reconfiguration, forced table reload, lock reset,
flash writes or watchdog changes are introduced. The underlying ROM call still
uses force_reload=false and the same work area. The extra uncached read and
marker delays may change reproducibility.

| Cyan LEDs | Last completed/entered point |
|---|---|
| 1 | In RAM, about to read one uncached, untranslated word at flash offset zero |
| 2 | That flash read completed (does not validate its contents) |
| 3 | ROM function lookup returned a non-null pointer |
| 4 | SHA lock acquired; about to call the ROM partition loader |
| 5 | ROM returned to RAM and its result was saved |
| 6 | SHA lock released; about to return to flash-backed launcher code |
| 7 | Flash-backed caller resumed; about to release diagnostic PIO resources |

| Magenta LEDs | Explicit diagnostic stop |
|---|---|
| 1 | ROM lookup returned null |
| 2 | SHA lock unavailable |
| 3 | ROM returned a negative error (lock has been released) |

Magenta deliberately stops rather than hiding an error by continuing to chain.
If the diagnostic cannot claim PIO resources it remains one blue. Therefore one
blue in v3 also includes PIO setup; it does not identify a partition loader hang.
Four cyan narrows the interval to the ROM call/return and following RAM marker;
six cyan suggests investigating return-to-flash, but neither proves root cause.
LED/timer hardware itself is part of the diagnostic and could also fail.

All four images built; combined UF2 checks passed (11,779 payload blocks).
Compiled caller, LED sender and error-stop routines and their literals were
verified in SRAM, with no direct calls into flash. Existing LED ownership,
5 ms polling and Wi-Fi gate tests passed. C64 idle helper remains 822 bytes in
SRAM, dispatcher in flash, static heap headroom 32,564 bytes. Hardware testing
is pending. This is a diagnostic build, not a confirmed warm-reset fix.

## Diagnostic v4: USB/stdio startup

V3 hardware result: first Reset left all green latched; a second Reset left
three yellow/amber LEDs. On that second attempt the launcher handover and clock
setup returned. In Master the next call is stdio_init_all(), followed by four
amber. This identifies an interval, not a proven USB fault. The earlier one-blue
v2 result is retained as a separate observation.

V4 retains the v3 partition probes and adds linker wrappers around four original
SDK/TinyUSB functions in each machine image. The original functions execute once
with unchanged arguments and return values. No SDK files are modified and no
USB disable, extra controller reset, timeout or retry is added. Each marker
adds approximately 600 microseconds and PIO setup overhead. This can change timing.

| Red LEDs | USB startup progress |
|---|---|
| 1 | Entered USB stdio wrapper, before SDK stdio_usb_init |
| 2 | Entered TinyUSB root-port initialisation wrapper |
| 3 | Entered USB device-controller initialisation wrapper |
| 4 | About to run hardware USB initialisation (controller reset/release, DPRAM clear, PHY mux) |
| 5 | Hardware USB initialisation returned |
| 6 | Device-controller initialisation returned |
| 7 | TinyUSB root-port initialisation returned; remaining stdio setup follows |

One **white** LED means stdio_usb_init returned, regardless of its boolean result.
Four amber then means stdio_init_all returned. Green markers follow as before.
A marker reports the latest observed boundary; the LED sender or following code
can also be involved in a failure. In particular red four does not uniquely
identify the controller reset wait within the hardware routine.

Validation: all four images built, 11,781 combined UF2 payload blocks verified.
Disassembly confirms calls into all four wrappers in Master, Model B and C64,
and that each wrapper calls its original function. Sanitizer-backed host tests
check marker order, arguments and true/false return propagation. LED tests,
5 ms polling test, Wi-Fi gate test and launcher RAM-placement checks passed.
The D81 helper remains 822 bytes in SRAM, dispatcher in flash, and C64 static
heap headroom remains 32,564 bytes. Hardware validation is pending.

Flash v4, power-cycle, run D81 through the mountain screen and leave it running
long enough to reproduce the failure. Press physical Reset and report the final
colour/count; if another Reset is needed, report that separately. The normal
successful final indication remains seven green LEDs. No confirmed fix is claimed.

## Diagnostic v5: before launcher and machine main

V4 hardware reports include a successful reset and a second-reset stop at five
blue. The user clarified that recent first resets appear to leave the previous
green LEDs latched; blue markers appear after a second reset. Do not combine
those attempts into one failure location or infer progress from latched LEDs.

V5 retains all earlier probes. It registers two additional SDK preinitializers
and wraps entry to main in the launcher and all machine images. It does not
replace the runtime initializer or skip any SDK initialisation. The first probe
runs at order 01101, after clocks, post-clock peripheral resets, spin locks and
mutexes; the second at 11001, after default alarm-pool setup. A final probe runs
after runtime_init returns, immediately before calling the original main.

| White LEDs | Startup progress |
|---|---|
| 1 | Existing v4 USB stdio return marker |
| 2 | Launcher preinitialisation reached the clocks/locks-ready probe |
| 3 | Launcher passed default alarm-pool initialisation |
| 4 | Launcher runtime returned; about to enter launcher main |
| 5 | Machine preinitialisation reached the clocks/locks-ready probe |
| 6 | Machine passed default alarm-pool initialisation |
| 7 | Machine runtime returned; about to enter machine main |

These are not reset-vector probes. ROM boot, XIP setup, data/BSS initialisation
and the earliest SDK initialisers precede the first white marker. An unchanged
green indication cannot distinguish a stall in any of those from failure in the
marker itself. The marker requires initialised clocks, timer, PIO and locks;
using it at reset-vector entry would be unsafe. The three added markers per
image add timing/PIO activity and may affect reproducibility.

Validation: all four images compiled; ELF tests verified actual preinit-table
ordering and the CRT pointer to the main-entry wrapper in every image. Existing
LED, USB wrapper, RAM partition-probe, 5 ms polling and Wi-Fi gate tests passed.
Combined UF2 verified at 11,781 payload blocks. C64 idle helper remains 822 bytes
in SRAM, dispatcher remains in flash and static heap headroom is 32,564 bytes.
Hardware testing is pending; this is not a confirmed reset fix.

Test the first and second resets separately. After flashing and power-cycling,
run D81 for the longer interval that triggers the problem. Press Reset once,
note the final colour/count and whether the display/web recovers. Only then
press Reset a second time and record that outcome separately. A short video of
the LEDs can capture markers that appear briefly before the final state.

## V5 withdrawn; diagnostic v6

Hardware: v5 produced a blank screen with no LEDs on boot. Restoring v4 restored
operation. V5 must not be used for further testing.

Source and disassembly identified a concrete ordering defect in v5. Its ordinary
preinit entries 01101/11001 preceded the SDK's per-core coprocessor initializer
in `.preinit_array.ZZZZZ.00200`. The LED helper contains VFP conversion, division
and multiplication instructions. Executing those before CP10 is enabled can fault
before the first LED output. Earlier ordering tests checked clocks/locks but
missed this prerequisite. This strongly explains the v5 regression, but no
on-board fault-register capture was obtained. It does not establish the cause
of the original warm-reset failure.

V6 removes both early ordinary preinit probes and registers one per-core probe
at ZZZZZ.01201, after coprocessor enablement and default IRQ priorities. It skips
core 1 explicitly. All ordinary SDK initialization (including the alarm pool)
now precedes this probe. The main-entry wrapper is retained. No SDK setup is
reordered or bypassed. The USB, partition and machine-main markers remain as v4.

| White LEDs in v6 | Progress |
|---|---|
| 1 | USB stdio returned (existing v4 marker) |
| 2 | Launcher late preinit reached, after coprocessor setup |
| 4 | Launcher runtime returned; original main is next |
| 5 | Machine late preinit reached, after coprocessor setup |
| 7 | Machine runtime returned; original main is next |

White counts 3 and 6 from v5 are retired. The interval between white 2/5 and
white 4/7 includes remaining runtime work and constructors. An unchanged green
state still does not locate a failure before the first marker. Marker timing
can affect reproduction; no warm-reset fix is claimed.

Validation: all images compiled; actual ELF preinit tables verify the new probe
follows coprocessor and IRQ setup. Core-1 suppression and main return propagation
passed sanitizer-backed tests. LED, USB wrapper, partition SRAM placement,
5 ms polling, Wi-Fi gate and combined UF2 checks passed (11,781 blocks). C64
helper placement and 32,564-byte static heap headroom are unchanged.

First hardware test: flash v6 and power-cycle, confirming the normal display,
web interface and final green LEDs before attempting a prolonged D81 run. Keep
v4 available as the known booting diagnostic. V6 hardware validation is pending.

## V6 comparison A: pre-main probes disabled

User reports repeated successful warm resets with unchanged v6, including a
12.5-minute C64 uptime sample with D81 active. Original intermittent failure has
not been reproduced in those v6 tests; this is not a confirmed root-cause fix.
The exact v6 UF2 is retained and its checksum reverified.

Comparison A is built with STARTUP_DIAG=ON and STARTUP_DIAG_PREMAIN=OFF for all
four images. This excludes startup_runtime_diag.c and --wrap=main, retaining the
SRAM partition-call path, USB wrappers and all main/later markers. The new option
defaults ON when diagnostics are enabled, preserving v6 behaviour unless explicitly
disabled. It has no effect when STARTUP_DIAG is OFF.

White 2/4/5/7 pre-main markers will not appear. White 1 (USB stdio returned) remains.
Blue/cyan/magenta partition, amber machine entry, red USB and green later markers
are unchanged. No D81 scheduling, 5 ms polling, Wi-Fi or Nimbus logic changes.

All four images compiled. ELF symbol checks confirm absence of runtime probes
and the main wrapper, presence of partition/USB probes, and RAM placement of the
partition caller. 5 ms polling, Wi-Fi gate, UF2 packaging and D81 helper placement
checks passed. The idle helper remains 822 bytes and C64 static headroom remains
32,564 bytes. Hardware validation pending.

Repeat a comparable cold boot, long D81 run and physical Reset. Record first and,
only if necessary, second Reset separately. If the failure returns, a repeat with
the exact retained v6 is the useful control. Removing probes also changes code
layout, so a difference implicates the instrumentation rather than proving a
particular delay or race condition. No publication or commit has been performed.


## Controlled launcher comparison, 6 November

Experimental firmware; hardware validation pending. Not a confirmed reset fix.

Restores the exact saved v6 launcher payload by preserving its original source/link order. All three machine payloads are byte-identical to the previous Nimbus build 8 candidate. Verified by address-by-address UF2 payload comparison, not just matching source options. Original v6 and build 8 artifacts are unchanged.

The previous option refactor reordered launcher objects; 372 launcher payload bytes differed from saved v6. This confounds earlier claims that only the radio timeout changed. It does not prove code layout caused the reset fault. Long v6 tests also used retries disabled whereas the latest failed build 8 test used retries enabled.

Preserves 5 ms polling, D81 optimisations, RAM partition-call path, startup probes, Wi-Fi gate, Nimbus backoff and 2.56-second paging timeout. Bluetooth status still reports build 8. Identify this test by its UF2 filename and checksum.

Build completed. RAM partition placement, runtime probe ordering, wrapped main and git whitespace checks passed. Machine payload identity preserves the preceding build's tested code.

Test: flash, power-cycle, confirm normal boot. Use 8bitretro.local, Nimbus off and automatic retries enabled, matching the failed build 8 test. Run D81 to the mountain screen and leave it for at least 45–50 minutes. Press physical Reset once; note boot/web recovery or exact LED count/colour. If hung, record a second press separately. Power-cycle to recover if necessary. Retain original v6 as reference.

## Checkpoint: 6 November 2026, controlled build hardware result

The v6-launcher/build-8-machines comparison completed a D81 run at uptime
2,727,127 ms (45 minutes 27 seconds), Nimbus off with automatic retries enabled.
The user then reported a successful boot on the first physical Reset. Post-reset
web recovery has not yet been explicitly confirmed. This is one successful trial,
not a proven warm-reset fix; repeat the same long run before changing the build.

Pre-reset: 82% speed, 206 frames/5.000417 s, core 0 98%, core 1 67%, 250 MHz,
34 C. Wi-Fi had one join, three link transitions and no recorded connection
errors, expirations or rejections. Bluetooth build 8 had 46 failed outgoing attempts
with the controller off. Free RAM was 26,232 bytes with three HTTP connections.
These readings do not establish absence of memory corruption or future outages.

Retained test artifact: rp2350-8-bit-retro-v6-launcher-build8.uf2
SHA256: 6e82d786a44c14d7e7870cc25d286ce07556cce5e4bc674a0253d9f2e9ad5864
The original v6 reference remains unchanged. Committing source changes does not
replace the hardware-tested artifact; a fresh build may carry different Git
version metadata. Continue tomorrow with the retained binary for a controlled test.

Outstanding: intermittent warm-reset failure remains unexplained; launcher layout,
probe timing/cleanup and radio activity are not isolated causes. Shorter Nimbus
paging improves some response delays but has not eliminated all browser timeouts.
The two-white checkpoint includes probe cleanup and remaining startup code and
cannot by itself identify a failing operation. D81 remains below full speed.

This experimental checkpoint preserves the prior D81 optimisations, 5 ms polling,
Wi-Fi boot gate and Nimbus retry backoff. STARTUP_DIAG defaults OFF; the tested
build enables it and STARTUP_DIAG_PREMAIN. The RAM partition-call diagnostic path
is conditional on STARTUP_DIAG, not a general production reset fix.
