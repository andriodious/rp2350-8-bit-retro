# Verified four-machine startup and DMA recovery checkpoint

Hardware verified by the user on 22 November 2026. Reference:
`reference/four-machine-startup-dma-20261122`.

This checkpoints the exact combined UF2 tested on the Presto, without rebuilding
it. It contains the network timer-pool correction, early C64 startup watchdog,
safe display DMA recovery ordering, and retained C64 terminal-caller diagnostics.
The D81/SID optimisations, touchscreen menus, Wi-Fi, keyboard and Nimbus code are
preserved. v0.8.0 and all earlier reference checkpoints remain unchanged.

| Machine | Tested image ID |
|---|---|
| BBC Master | `94b748eb89fceb1e` |
| BBC Micro | `3eb96fee51390a3d` |
| C64 | `e4e9971288b6eadd` |
| ZX Spectrum | `c0652c3c18d02d2b` |

Combined firmware: `retro8bit-startup-dma-fix-test.uf2`.

SHA-256: `b761a3a64304b67294fb6fcacea3784d83c45539a118e804a5068db37e91c14d`.

Launcher SHA-256: `4f64f88f7b5f22f9d7f0409459142639f6a7045db5d71802d2757f2f14d868fa`;
the launcher is byte-identical to the preceding candidate and v0.8.0 pair.

## Hardware results

- C64 physical resets from Elite and Monkey Island D81 returned to C64 Elite and
  restored the web interface, with no fallback to Master. The number of reset
  attempts was not specified.
- Settled Monkey Island delivered 6,737 emulated and rendered frames over
  134.741 seconds, approximately 50.00 fps, with no additional audio underruns.
  Free memory held at 18,872 bytes after an earlier 1,744-byte drop, and returned
  to 24,456 bytes following reset to Elite.
- Spectrum, BBC Micro and Master passed startup, display, menu resume, sound and
  web responsiveness checks. Spectrum's sampled windows were approximately
  50.08 fps with no rendering stalls or recorded fault.
- The latest return to C64 still reports the expected image, 24,456 bytes free,
  sound on, turbo off, and no recovery or fault record.

The separate timer-only image `d6732ae6816431fa` reached 30 minutes 53 seconds
without recovery before the reset-test firmware was flashed. Its audio underrun
counter did increase. That observation is not a soak result for this checkpoint.
Bluetooth reconnection was not newly verified in this test sequence.

These finite passing tests do not prove that every intermittent physical-reset
failure has been eliminated. The watchdog coverage begins at `main`, not in the
boot ROM or earlier runtime startup. Detailed evidence is in
[verification.json](verification.json).

## Build and retained evidence

The tested build used the normal four-machine recipe with caller capture enabled:

```sh
. deps/env.sh
EXTRA_OS_CMAKE_ARGS='-DC64_ABORT_DIAG=ON' scripts/build-classic-keyboard-four.sh build-startup-dma-fix
```

Use the retained UF2 when reproducing these hardware results. A fresh build can
have different image IDs and is not automatically hardware verified. Keep the
matching retained C64 ELF when decoding terminal caller addresses.

The local archive `artifacts/four-machine-startup-dma-verified-20261122` beside the
checkout contains the unchanged tested UF2, all five ELF files and maps, original
build settings and log, checksums, source patch, and hardware verification record.
The original candidate archive remains intact. Firmware binaries and ROMs are
not added to Git by this checkpoint.

Host checks already passed for early startup timeouts, launcher recovery, all
four display formats/configurations, network timer exhaustion, caller capture,
startup banner, paused menus and live LED settings. Packaging checks verified all
15,085 payload blocks and 28,792 bytes of C64 static RAM headroom. See
[the diagnostic notes](../../docs/diagnostics/startup-dma-fix-20261122.md).
