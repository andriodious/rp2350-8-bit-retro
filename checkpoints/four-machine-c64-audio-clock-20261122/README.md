# Verified C64 audio clock checkpoint

Hardware verified by the user on 22 November 2026. Reference:
`reference/four-machine-c64-audio-clock-20261122`.

This preserves the exact combined UF2 tested on the Presto, without rebuilding
it. The only firmware source change from v0.9.0 selects a fractional DMA audio
timer rate at startup. At the normal 250 MHz clock it uses 11/62358, approximately
44,100.196 samples per second, instead of 1/5668 (44,107.269). SID production is
approximately 44,099.945 samples per second, leaving a small residual mismatch.
No samples are inserted and underrun accounting is unchanged.

Original SID state/history processing, PCM conversion, volume arithmetic and
buffer sizes are retained. The startup watchdog, display DMA recovery ordering,
network timer-pool correction, terminal-caller capture, D81 optimisations,
touchscreen controls, Wi-Fi and Bluetooth support are unchanged. The rejected
SID/PWM optimisation candidate is not part of this checkpoint.

| Machine | Image ID | Verification |
|---|---|---|
| BBC Master | `94b748eb89fceb1e` | Exact unchanged v0.9.0 payload |
| BBC Micro | `3eb96fee51390a3d` | Exact unchanged v0.9.0 payload |
| C64 | `a9b493c27aacb283` | New hardware tests below |
| ZX Spectrum | `c0652c3c18d02d2b` | Exact unchanged v0.9.0 payload |

Combined firmware: `retro8bit-c64-audio-clock-only-test.uf2` (7,724,032 bytes).

SHA-256: `8d929614938bf490e941851b59a9383af3ae8d7e6520dc8dc02ab7b89e3f7243`.

Launcher SHA-256: `4f64f88f7b5f22f9d7f0409459142639f6a7045db5d71802d2757f2f14d868fa`;
the launcher also remains byte-identical to v0.9.0.

## Hardware results

All measured intervals used turbo off and sound on at volume 100.

- Settled Elite: 1,795 emulated and rendered frames in 35.899 seconds,
  approximately 50.0014 fps. The underrun counter increased by 19 (0.5293 per
  second). CPU readings were 68% / 35–36%; free memory stayed at 24,456 bytes.
- Settled Monkey Island D81: 2,187 emulated and rendered frames in 43.740 seconds,
  exactly 50 fps, with zero additional underruns. Free memory stayed at 20,616
  bytes. Core 0 read 75%; core 1 readings were 50% and 40%.
- The user confirmed that physical resets passed and sound and web
  responsiveness were fine. The reset count was not specified.
- A subsequent post-reset sample confirmed the same C64 image, 100% speed,
  recovery false, no recorded fault, sound on, and 22,712 bytes free. Its lifetime
  underrun count was 506 at 17.509 seconds; this is not a steady-state rate.

The longer D81 intervals included user-confirmed menu pauses. The audio missing-
sample counter also increases during intentional menu silence, and the D81
poll-gap counter includes time away from the drive/raster loop. Those intervals
are excluded from the settled performance comparison. Their evidence is retained
in the local archive.

These are finite tests, not proof of zero underruns or of eliminating every
intermittent reset failure. Bluetooth reconnection and the other three machines
were not newly hardware-tested in this audio sequence; their unchanged payloads
retain the previous v0.9.0 verification. Detailed measurements and the latest
reset sample are in [verification.json](verification.json).

## Host verification and retained build

The full production driver passes the new host harness with AddressSanitizer
and UBSan, with diagnostics both on and off. Checks include all 13,172,736
signed-sample/volume combinations per configuration, queue wrap/overflow,
mute/tone behavior, DMA/IRQ setup/shutdown, and an exhaustive clock-fraction
oracle at 21 clock frequencies. Long-duration, pause and slow-producer models
are documented with their assumptions in
[the harness notes](../../tests/c64/audio_host/README.md). Existing early-watchdog,
audio-stop and four-machine display DMA checks also passed on this source.

Independent package validation checked all 15,086 UF2 blocks: 10,477 non-C64
blocks match v0.9.0 exactly and 4,609 C64 blocks match the rebuilt C64 input at
its slot address. No overlap or extra addresses were found. Original generated
SID source and hot SID/CPU/VIC SRAM addresses and sizes were retained.

The local archive `artifacts/four-machine-c64-audio-clock-verified-20261122`
beside the checkout contains the unchanged tested UF2, component images,
matching C64 ELF/map, original build configuration/logs, source patch, checksums,
and host/hardware evidence. The original candidate archive remains intact.
Use this retained binary to reproduce the hardware results; a fresh build can
have a different image identity and is not automatically hardware verified.
Firmware binaries and ROMs are not added to Git.

This is a local checkpoint. v0.9.0 and all earlier checkpoints remain unchanged;
no release or remote branch is updated by this checkpoint.
