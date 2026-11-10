# Clean combined warm-reset reference — 2026-11-10

## Exact checkpoint

Source commit: c3c3c2b (clean detached build). Reference tag: reference/clean-combined-38533c78b520f911.
Artifact directory: ../artifacts/combined-clean-c3c3c2b (relative to repository).
UF2: presto-combined-clean.uf2
SHA256: 9a189a13d0dc791cdef59f5f510695a0a99e8829bb37de067623a847ff538ac8
C64 image: 38533c78b520f911; Master: 92852038d2174937; Model B: f1c2a7bca0c44e53.
Launcher UF2 SHA256: e9409e3b818c80e26a4ef569f8aad79cfa30a517474cbc1f0a6a72832f7575bc.
The directory preserves machine/launcher UF2s, ELFs, CMake caches, identity headers,
build log and SHA256SUMS. All manifest checksums reverified at checkpoint.
The tag identifies source; the UF2 hash identifies the exact tested binary.

User reports no warm-reset issues with this combined build on two Prestos.
User additionally reports standalone C64 failed warm reboot on almost every reset.
Follow-up clarification: the latest standalone diagnostic build seemed worse.
This was not a confirmed failure of clean standalone 248af4fedbeca14f. Exact reset
counts, durations and the diagnostic image ID were not supplied. Do not treat
this as a controlled failure-rate measurement. Clean standalone remains the next
useful comparison when the user has time; no further hardware testing is pending now.
Stable release untouched. No firmware behaviour changed for this checkpoint.

## Build differences and implications

Compared saved clean C64 cache/ELF with archived serial-wait diagnostic
3bb77c71ef261c1e and inspected reset investigation source changes.
Older combined restore contains C64 5dd9402c94581aa5 and diagnostic launcher
SHA a7a8edcfbceacbcdfd457b13391920d64c1f8affb4be6c4973c1663d5719d83f.
It predates later D81 work: it is not a one-variable control.

| Area | Clean reference | Diagnostic builds | Interpretation |
| --- | --- | --- | --- |
| Boot layout | Launcher plus three slots | Some combined, later standalone | Major confound: external reset normally selects Master in combined; standalone starts C64 directly |
| Startup LED probes | OFF | STARTUP_DIAG/PREMAIN ON | PIO0/GPIO33, float clock calculation, blocking FIFO writes and 600us waits before normal board setup; not passive logging |
| Launcher partition load | SDK rom_load_partition_table | partition_diag_load when STARTUP_DIAG | Adds uncached physical flash read, active PIO and SRAM execution around ROM call; explicit permanent stops on unavailable resources/lock/error |
| Earliest C64 probes | Absent | Later CRT replacement/backlight probes | GPIO bank reset deassertion and GPIO45 pulses before normal CRT/runtime; changes ordering/timing substantially |
| C64 startup pauses | Absent | Later white sequence and 120ms checkpoints | Alters peripheral initialization timing |
| USB stdio | Normal initialization | Older wrappers/IRQ deferral; later initialization skipped | Restored USB behaviour is a separate variable from removing diagnostics |
| Memory guard | Absent | Later stack limits, emergency fault stacks, incremental code/vector checks | Changes SRAM layout, fault handling and runtime traffic; no detected corruption in supplied status |
| Flash wrappers | Absent | Later erase/program/lockout records | Supplied pre-reset counters were zero; no evidence of a monitored flash operation causing those failures |
| Profiling / web tracing | OFF | C64_PROFILE and WEBDIAG ON | Changes CPU/PSRAM/XIP traffic and timing while C64 is running |
| Serial CLOCK wait shortcut | OFF | ON in 3bb77c71ef261c1e | Additional confound; does not explain failures with Elite and no D81 |

Both SDK and diagnostic partition loaders use the same SHA-256 ROM lock and
release it on normal return. The lock itself is not unique to diagnostics.
The diagnostic path halts on failures where the ordinary launcher proceeds;
this is a candidate explanation, not evidence that those branches were reached.
Standalone failures mean launcher instrumentation cannot explain every failure.
Seven green LEDs may be retained from before reset; they do not locate execution.

Clean C64 ELF size totals: text 1,155,852; BSS 429,344 bytes.
Serial-wait diagnostic: text 1,161,212; BSS 430,792 bytes.
These are ELF section totals, not live heap or available SRAM measurements.
The diagnostic adds 5,360 text and 1,448 BSS bytes; layout/cache effects remain possible.

Retained: SDK 2.2.0, 250MHz, flash slowdown before clock increase, D81 batching,
SID SRAM state/hot code, loading SRAM, ROM delay/job scan, normal watchdog,
Wi-Fi/Nimbus and web starting-screen changes. No new peripheral-reset fix,
SDK lockout patch or flash-timing change was introduced in the clean build.

## Priority of explanations

1. Different boot destination/path (combined external reset to Master versus
   standalone direct C64) must be separated from instrumentation effects.
2. Early PIO/GPIO/CRT/USB instrumentation changes actual startup execution.
   Removing it is the strongest code-level diagnostic-related lead, not a proven cause.
3. Runtime tracing/checks and altered layout may change the hardware/cache state
   or timing when physical reset arrives.
4. Flash-write lockout remains unsupported by captured counters; memory corruption
   remains unproven. Neither is universally excluded by the limited checks.

## Controlled next comparison

Preserve this exact combined UF2. First clarify whether the reported near-every-reset
standalone failures include clean standalone 248af4fedbeca14f or only diagnostic images.
Compare clean standalone and clean combined on the same board, USB host/cable,
Elite runtime, sound/Nimbus settings, and physical-reset procedure. Record the
machine that appears after reset and pass/fail counts; preserve the first failure.
If clean standalone fails while clean combined succeeds, prioritize direct C64
startup versus launcher/Master handoff, not wholesale reintroduction of diagnostics.
If both clean builds succeed, the first optional diagnostic A/B should add only
application startup LED probes, with pre-main probes disabled and the clean launcher
unchanged. Later separate pre-main probes, USB changes, runtime profiling and guards.
Do not bundle these switches again. Hardware/SWD evidence is needed to locate a hang.

Checkpoint validation: exact artifact hashes verified; saved configuration comparison;
source inspection of SDK/launcher/CRT/USB/guard paths; existing full build and targeted
audio, poll-budget, Wi-Fi gate, web-switch, UF2 payload/bounds and SRAM placement checks
remain recorded in the artifact README. No additional hardware tests performed locally.
