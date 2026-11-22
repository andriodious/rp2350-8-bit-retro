# Network timer-pool test candidate

This is a separate four-machine candidate based on v0.8.0, with the C64 terminal
caller diagnostics retained. No published release or reference tag is replaced.

The local test uses the pinned lwIP implementation, including its actual mDNS
receive callback, timer allocation, timers, packet pools and TCP timer. Packets
are supplied in memory; it opens no sockets and does not access the Presto.
Host adaptations are a simulated clock and network output, deterministic random
delay, 64-bit pointer alignment, assertion reporting and timer-list inspection.

The former reserve of four timers above the seven internal timers fails when
mDNS multicast suppression overlaps pending truncated questions. It asserts:

```text
sys_timeout: timeout != NULL, pool MEMP_SYS_TIMEOUT is empty
```

In the retained v0.8.0 C64 ELF this panic reaches the same `_exit` breakpoint at
`10020ee4` seen in the hardware failure. This identifies a reproducible defect
and matching terminal path, but does not establish which network event caused
the original hardware reboot from that earlier record alone.

The subsequent hardware failure on diagnostic C64 image `f75350f0fc1f619b`
confirmed this assertion: Master image `2dbd7a6814f5bae4` retained `kind=5`
(panic), `core=0`, `phase=6`, `frontend_stage=6` (network),
`caller=10033b59`, and `detail=10068fa4`. The matching retained ELF decodes the
caller to `sys_timeout_abs` at `timeouts.c:190` and the detail pointer to the
exact empty-pool message above. Timer-pool exhaustion is therefore the confirmed
immediate cause of this hardware crash. The record does not identify the
individual packets or all timer owners.

For the current single IPv4 interface the required maximum is:

| Timer use | Entries |
|---|---:|
| Internal timers, including the shared TCP timer | 7 |
| Stored truncated mDNS questions | 4 |
| Multicast suppression | 3 |
| Delayed multicast and unicast replies | 2 |
| Probe, announcement or restart | 1 |
| Total required | 17 |

The configuration now provides 19 entries, including two spare entries. The
packet-slot limit is explicitly four and participates in the timer expression.
Increasing IPv6 or the number of network interfaces requires revisiting this
budget. No SDK source or emulator loop is changed.

Run `python3 tests/network/mdns_timeout_test.py` after dependency setup. It
compiles the real lwIP twice with AddressSanitizer and UndefinedBehaviorSanitizer:

- Old budget, spaced requests: passes, peak 11/11.
- Old budget, overlapping requests: reproduces the exact assertion.
- New budget, the same overlap: passes, peak 14/19.
- New budget, both delayed reply kinds, all four pending questions, suppression
  and restart together: reaches 17/19 without exhausting the pool.
- 100 repeated lifecycle rounds, 707 queries total: passes, returns to the same
  settled timer count each round; excess pending questions are handled safely.

The callback/HTTP lifetime, C64 network polling, Wi-Fi/Bluetooth gate and C64
terminal-diagnostic host tests also pass. These tests do not simulate the radio
bus, DMA, physical memory timing or real multicore execution.

The separate timer-pool build `d6732ae6816431fa` subsequently reached 30 minutes
53 seconds without recovery; free memory remained 24,456 bytes. Audio underruns
did increase during that run. The fix is also included in the
[hardware-verified startup/DMA checkpoint](../../checkpoints/four-machine-startup-dma-20261122/README.md),
which passed C64 Elite/D81 physical resets and the other machines' startup,
display, menu, sound and web checks. These observations support the fix but do
not reproduce every possible network workload. If C64 returns to Master again,
save `/api/status` before resetting or switching machines.
