# C64 versus Master warm-reset investigation — 8 November 2026

## Reproduction and limits

On identity build C64 053082e1c0cd9ce0 / Master 23f480399617eba6,
repeated Master physical resets pass. C64 Elite after a few minutes can leave
black display, seven green LEDs and no web access; a second reset does not recover,
a power cycle does. D81 execution and long runtime are not prerequisites.
Seven green LEDs can be retained from before reset, not proof of new startup progress.

## Source comparison

- Both main functions set flash M0 CLKDIV=3 and RXDELAY=2 from SRAM before
  changing voltage to 1.25 V and clock to 250 MHz. No C64-only QMI flash mode
  change was found in the compiled integration.
- Both call the same board_psram_init / psram_setup implementation. It exits
  QPI, sends reset-enable/reset, waits, enters QPI and configures M1 read/write.
  PSRAM_MAX_FREQ_MHZ=125 in C64 is not consumed by this board implementation;
  it uses PSRAM_MAX_HZ=133000000. At 250 MHz its divisor is 2: both use 125 MHz.
  Do not mistake the imported FRANK main_rp2350.c setup for active code: that
  main is not in the Presto target.
- Both use the shared LCD/PIO driver, board init, USB initialization and
  network implementation. C64 has a different frame producer and memory traffic.
- C64 audio uses 32-bit, 1024-byte-ring DMA buffers and its own SID producer;
  Master uses the shared SN audio path. Muting C64 does not stop audio DMA.
  DMA differences are candidates to isolate, not demonstrated reset defects.
- C64 has a 16 KiB core-0 stack at 0x2007c000..0x20080000, with main SRAM reduced
  to 496 KiB. Master uses the normal scratch-bank stack. The launcher reuses the
  upper main SRAM, but ordinary overlap across separate boots is not evidence
  of corruption: its initialized data/BSS are established at startup.
- C64 explicitly arms an 8-second watchdog twice and feeds it while running.
  Master main does not call boot_guard_begin or watchdog_enable. Do not infer
  Master's watchdog policy from the common boot_guard implementation (Beeb
  invokes that guard). The SDK sets PSM watchdog selection and scratch[4] when
  enabling. C64 also writes diagnostic scratch[0..3]. The reference ROM uses
  scratch[4..7] for its boot vector, so no direct diagnostic-magic collision was
  found. This does not prove the watchdog causes the physical-reset failure.

## Controlled next experiment

C64_WATCHDOG defaults ON. The comparison build sets it OFF, suppressing the two
C64 watchdog_enable calls. Existing watchdog_disable, feeds and diagnostic scratch
writes are retained to limit the change. Feeding a disabled watchdog does not
arm it. Software-requested reboots can still use the SDK reboot path.

The diagnostic loses automatic C64 watchdog recovery: a fault can require manual
reset or power cycling. This is not a production fix. All D81, 5 ms networking,
Wi-Fi/Nimbus settings and launcher probes remain intact. The launcher and BBC
machine UF2 inputs are reused from the identity build, not rebuilt.

Power-cycle after flashing. Keep Nimbus off and retries enabled. Run C64 Elite
3–5 minutes, capture status, then physical Reset once and check display/web.
Record a second press separately if hung. A failure weakens the watchdog-enable
hypothesis; a pass needs repetition and a return to the enabled reference because
recompilation also changes code layout. If it persists, isolate C64 audio DMA next,
then PSRAM traffic; avoid a speculative flash-reset sequence before locating the
failure. SWD inspection during a hang would distinguish ROM from launcher/runtime
more directly than retained LEDs.

## Runtime comparison

Three watchdog-off resets restored display/web; returning to enabled reference
reproduced black screen/seven green/no web. Sound was off for the first disabled
pass, on for the other two, off on the enabled failure. These are suggestive, not
proof; code layout differs.

# Same-binary watchdog comparison

C64 image: 302a2c94bc84f5c9. Default watchdog enabled. Diagnostic only; no reset fix claimed.
Launcher, Master and Beeb payloads verified identical to the identity reference.

Flash and power-cycle. Select C64, then reload the page to fetch its updated web UI
(the unchanged Master page does not contain the new controls). Expand Diagnostic
trace. Diagnostic watchdog has Enable/Disable buttons and a hardware-state label.
Status contains c64_watchdog.enabled and toggle_allowed; trace retains both.

First test with sound off, Nimbus off/retries enabled: click Disable, confirm
c64_watchdog.enabled=false, run Elite 3–5 minutes, physical Reset once, check
Master display and web. Repeat with Enable on the exact same firmware. Each machine
restart defaults enabled. Save pre-reset status for each run; no reflashing between
conditions. Disabling watchdog removes automatic crash recovery. It does not undo
PSM watchdog selection or scratch writes already performed during boot, so this
test isolates the running enable state, not every side effect of initialization.

Validation: C64 build passed; actual handler/setter sanitizer tests passed for both
states, invalid requests and fault/recovery rejection. Combined UF2 packed and BBC/
launcher payload equality verified. Browser/hardware interaction still needs testing.

## Runtime-disabled watchdog also failed

User confirmed image 302a2c94bc84f5c9, c64_watchdog.enabled=false at 193,198 ms,
sound off and retries enabled. Physical Reset still produced black display, seven
green LEDs and no web; power-cycle restored operation. Runtime watchdog disable
is not sufficient. Initialization side effects remain untested. Keep watchdog
recovery enabled by default.

# C64 audio-hardware isolation

Image ID: 785977849de0e936. Experimental, no reset fix claimed. Watchdog defaults ON.
Launcher and both BBC payloads verified byte-identical to identity reference.
D81, 5 ms polling, Wi-Fi gate and Nimbus retry changes retained.

Power-cycle after flashing. Select C64, reload the web page, expand Diagnostic
trace. Keep sound off and Nimbus off/retries enabled. Leave watchdog Enabled.
Click Stop audio DMA; wait for c64_audio_hw.stopped=true and busy_mask=0 in status.
Run Elite 3–5 minutes then physical Reset once and check display/web. Audio blocks
should stop advancing. SID synthesis continues, so this isolates output hardware,
not all SID CPU work. Restarting C64 restores audio DMA; stopping is one-way for
this session. A second trial without Stop uses this same binary for comparison.

Requests are handed from core 0 to core 1 using atomic flags. Core 1 masks its
DMA IRQ, masks channel interrupts, stops the DMA request timer, clears both channel
enable bits before aborting either (SDK RP2350-E5 guidance), clears channel flags,
disables PWM and drives the audio pin low. Channels remain allocated to avoid reuse.

C64 compiled; sanitizer-backed shutdown ordering/idempotence and watchdog API tests
passed. Hardware/browser validation pending. This experiment cannot exclude effects
of having initialized audio earlier, or identify a pre-launcher failure by itself.


## 9 November: distinguish Reset-only USB entry from a hang

User confirmed a Reset-only seven-green-LED hang with no API or USB firmware drive (C64 Elite, watchdog enabled, audio DMA running, image 785977849de0e936), recovered by power cycle. Separately a Reset-only event after about 66 minutes with watchdog disabled entered USB firmware mode; do not presume accidental BOOT contact. Earlier ambiguous failures are not reliable watchdog statistics. Runtime/API-only web outages are tracked separately. Stop repeated watchdog comparisons and instrument launcher fallback; see launcher-fallback-report.md. Reference firmware is preserved.
