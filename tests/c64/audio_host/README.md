# C64 audio driver host checks

Run `python3 tests/c64/audio_host_test.py`. Use `--source /path/to/audio.c`
to compare an unchanged reference driver and `--output result.json` to save
the evidence. No hardware is opened and no firmware is flashed.

The harness includes the complete selected production driver. Only Pico
hardware APIs are mocked. AddressSanitizer and UndefinedBehaviorSanitizer are
enabled for both `WEBDIAG=0` and `WEBDIAG=1`.

Checks cover all 65,536 signed PCM samples at each volume from 0 through 200
(13,172,736 combinations per configuration), an independent 64-bit conversion
oracle, stereo averaging edges, packing, clipping, sound/status mute, speaker
test tone, FIFO capacity and overflow/drop semantics, 32-bit head/tail rollover,
DMA ring/chaining/initialization, IRQ acknowledgement/rearming and diagnostic
shutdown ordering. A driver providing `audio_clock_fraction` is additionally
checked against an exhaustive denominator oracle for 21 clock frequencies,
including 250 MHz and integer-limit cases.

Event-driven producer/drain models exercise ideal 44,100 Hz production using
smooth 1 ms sample batches and 882-sample PAL frame bursts for 600 seconds,
PAL bursts for two hours, a
sustained 26 fps producer for ten seconds, and a 20-second intentionally muted
pause followed by resuming production. The full-speed clock comparisons start
with **1,024 samples already buffered as a model assumption**; the firmware
does not add that prefill at startup. The slow-loader model starts with the
entire 4,096-sample ring full to demonstrate the finite limit of buffering.

Separate SID raster-rate models use the actual production fraction of
185265/65536 samples per line, 312 lines per frame and 50 frames per second,
retaining the fractional remainder across frames. This averages
44,099.945068359375 samples per second. They run for 600 and 7200 seconds with
both zero and 1024 prefilled samples. Production timing is still an assumption:
all 312 lines are processed in one burst every 20 ms. This reproduces the
fixed-point sample totals, not the actual hardware interleaving of CPU, SID
blocks and DMA. Results named `ideal_44100_` are deliberately distinct from
these SID raster-rate cases.

The missing-sample counter also increases before SID production starts and
while intentionally muted. Those counts do not establish audible failures.
The pause model validates that muted output remains at its neutral level while
the counter accumulates, then that production resumes normally.

These tests do not simulate concurrent CPUs, memory barriers, XIP/PSRAM latency,
LCD or radio contention, actual DMA request arbitration, IRQ deadlines or
analogue audio. The simulated counts explain deterministic clock mismatch and
producer starvation; they are not hardware performance measurements or proof
that all real audio interruptions have been eliminated.
