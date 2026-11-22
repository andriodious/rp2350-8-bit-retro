# C64 startup watchdog and display DMA recovery candidate

This separate combined test build is based on v0.8.0 with the network timer-pool
fix and C64 terminal-caller diagnostics retained. It does not replace v0.8.0,
any previous reference tag, or the retained timer-pool candidate.

Update, 22 November 2026: the user completed hardware testing and approved
[this verified checkpoint](../../checkpoints/four-machine-startup-dma-20261122/README.md).
The exact tested binary is retained; the original build-time candidate archive
has not been changed.

## Changes

C64 previously disabled the watchdog before USB, board, PSRAM and emulator
memory initialization, then enabled it after these operations. A stall in that
interval could leave a blank screen without an automatic recovery. The watchdog
now starts at entry to `main`, before changing flash timing, and receives a fresh
eight-second budget between startup stages. There is no new watchdog feed inside
hardware wait loops, so a stuck operation can expire even with interrupts off.

The previous reset record is captured before the watchdog is armed. These stage
numbers identify new early timeouts in the existing Master recovery status:

| Phase | Operation about to run |
|---|---|
| 10 | Flash timing and optional first startup marker |
| 11 | Voltage and system clock |
| 12 | USB stdio |
| 13 | Board setup |
| 14 | PSRAM setup |
| 15 | C64 memory setup |
| 1 | Display preparation and the existing startup path |

The SDK starts the watchdog tick from `clk_ref` before `main`; changing
`clk_sys` does not change its timeout. `C64_WATCHDOG=OFF` still disables the
feature. Standalone recovery preserves its original failure record while
initializing, then disarms the watchdog before serving its healthy recovery UI.
The existing launcher handles a C64 timeout by returning to Master. The launcher
itself is unchanged.

Display recovery previously aborted two linked row channels while other display
channels could still trigger them. RP2350-E5 requires disabling linked channels
before aborting. The recovery path now disables the entire display DMA graph
before its first abort, stops the relevant PIO state machines, waits for the
aborts, and then restores all pointers and transfer counts. It publishes any
pending row-table change and restarts at a known frame boundary. The display
timestamp uses the back-porch delay from that boundary, retaining BBC frame
pacing. The non-DMA timing configuration follows the same ordering for its two
row channels, leaving its independent timing engine running.

Normal and late-frame DMA paths are unchanged. An actual recovery may cause a
single visible resynchronization. No emulator CPU, drive, SID or pacing loop is
changed, and the existing D81/SID optimization options remain enabled.

## Local verification

The following tests run without contacting the Presto:

- `tests/startup/c64_early_watchdog_test.py`: compiles the actual startup and
  standalone recovery functions and the actual launcher decision body. A virtual
  watchdog injects hangs in all six early stages and at the display handoff,
  including a PSRAM hang with interrupts disabled. Checks enabled/disabled builds,
  fallback to Master, retained evidence, SDK scratch isolation, cooperative long
  waits and a recovery UI that remains available.
- `tests/startup/display_dma_restart_test.py`: executes the production display
  ISR, row publication and recovery helpers. Checks every initial busy-channel
  combination, source completions while disabling, pending writes during abort,
  pointer/count reconstruction, row-table changes and frame timestamp. All four
  machines pass with DMA timing on and off; normal and late paths are checked.
- The startup-banner and paused-frontend tests pass. Their host harnesses now
  account for the existing immutable SRAM scanline banks and disabled diagnostic
  marker, respectively; no production banner/menu behavior was changed.
- Existing launcher selection, watchdog toggle, C64 terminal diagnostic, menu
  rendering, live LED settings and real lwIP timer-pool regression checks pass.

The new watchdog and display tests run with AddressSanitizer and
UndefinedBehaviorSanitizer. The launcher selection test covers both reporting
configurations and malformed retained records. Build verification compares each
combined UF2 payload with its individual machine image, checks reserved storage
and C64 static SRAM headroom, and compares optimization options and launcher bytes
with the timer-pool build. Exact results and image IDs are in the accompanying
artifact manifest.

These are software sequence tests, not a reproduction of the old physical-reset
failure. They do not emulate the RP2350 bus, PIO execution, external flash/PSRAM,
or reset-button electrical behavior. Coverage starts at `main`; a ROM, XIP or
runtime startup failure before entry remains outside it.

## Hardware test outcome

The separate timer-only run on `d6732ae6816431fa` reached 30 minutes 53 seconds
without recovery, with unchanged 24,456 bytes free. Its underrun counter rose
from 651 to 13,996; that run is not a zero-underrun result or a soak test of this
startup/DMA candidate.

After flashing and power cycling the combined candidate, the user confirmed C64
image `e4e9971288b6eadd`. Physical resets from Elite and Monkey Island passed,
restoring Elite and web access without fallback to Master. Settled Monkey Island
ran at approximately 50.00 fps for 134.741 seconds with no added underruns.
Spectrum, BBC Micro and Master then passed startup, display, menu resume, sound
and web responsiveness checks. The user approved checkpointing the four-machine
build. Reset counts were not specified and Bluetooth reconnection was not newly
tested. These results do not establish that every intermittent reset failure is
eliminated; preserve `/api/status` before another reset if a future fallback occurs.
