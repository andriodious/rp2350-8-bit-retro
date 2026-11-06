# Temporary web-outage diagnostics

The latest traces show temporary loss of HTTP responses in Master as well as
C64, including with automatic Nimbus retries disabled. The user reports using
8bitretro.local and recovery without intervention. This does not establish
whether Wi-Fi disconnects, network servicing pauses, HTTP connections become
stuck, or the browser/network path stalls. The D81 shortcuts cannot directly
explain the Master-only outages.

Diagnostic builds (WEBDIAG) append `network` to full /api/status responses and
include it in downloaded web traces. Small legacy fetch-status fragments omit
this object. Counters are fixed-size, core-0 only, reset on machine restart;
no credentials, request bodies or client addresses are recorded.

- uptime_ms: board monotonic milliseconds, wrapping after about 49 days.
- link: sampled CYW43 TCP/IP link state (0 down, 1 joining, 2 no IP, 3 up;
  negative values indicate failure). Sampled at roughly 200 ms intervals when
  network polling runs, so very short transitions may be missed.
- joins: outgoing Wi-Fi join calls; transitions/link_ms: link changes and latest
  change time. links: last eight [uptime_ms, link-state] pairs in time order.
- ip_changes: changes to the interface IPv4 value, including acquiring/losing it.
- polls: actual cyw43_arch_poll calls through net_poll. poll_gap_us is the largest
  start-to-start gap; poll_cost_us is the largest duration inside that call.
  Measurements use an independent timestamp, not last_poll_us (which net_frame
  deliberately adjusts to force a poll). Restart shutdown polling is excluded.
- frame_gap_us: largest gap between net_frame entry calls, including startup.
- active/states: allocated HTTP connection count and [header, body, waiting for
  action, sending, done] counts. The current status request is included.
- oldest_ms: age of oldest allocated connection, not time since last activity.
- accepted/rejected/freed: accepted, rejected (capacity or allocation failure),
  and freed HTTP connections. errors/last_error: lwIP asynchronous error callbacks.
- evicted: unused pre-opened connections evicted at capacity; expired: quiet
  connections aborted by existing timeout handling.

Counters and maximum timings accumulate since restart, not per trace clear.
Gap/age measurements use uint32 microseconds, wrapping after about 71 minutes;
interpret gaps shorter than that interval. Poll cost is not HTTP response time.
Browser response metrics remain in `web`. An unchanged link history cannot by
itself prove the wireless path was healthy during a period with no sampling.

Start with Master, Nimbus retries disabled, a visible browser tab and a cleared
trace. Wait for an outage and natural recovery, without refreshing if practical,
then download the trace. The ring and cumulative counters retain evidence from
the interval without responses. Compare the last pre-outage and first recovered
samples. Repeat in C64 only after obtaining shared-path evidence. Do not change
retry policy or D81 performance settings during this comparison.

No network timeout/reconnect policy was changed. D81 whole-loop, instruction-tail
and small SRAM helper optimisations and Wi-Fi/Nimbus fixes are retained. Native
formatter tests cover maximum counters, ring ordering, small-buffer compatibility
and bounds with ASan/UBSan. Existing HTTP lifetime/abort tests and all three
firmware builds pass. Hardware diagnosis remains pending.

## Diagnostic build correction

The first delivered network-diagnostics UF2 omitted the network object because
WEBDIAG was supplied only to web-page generation, not the C compiler. The v2
build passes WEBDIAG explicitly to each board target. Integration validation
now checks the compile definition and formatter strings in all three ELFs.
The user's switch trace therefore cannot yet distinguish wireless link changes
from HTTP/server stalls. It does show automatic retries returning after machine
switches; disable again in the selected machine for the isolation run.

## Bounded C64 packet servicing candidate

The user's later Master-to-C64 trace showed no prolonged timeout after retries
were disabled, but a maximum actual network-poll gap of 877,711 us during the
C64 run and one sound request taking 2.77 s. The sampled Wi-Fi link stayed up,
IP/joins did not change, and HTTP capacity rejection remained zero. Startup
and outgoing Nimbus attempts still caused brief missing responses. These data
motivate reducing emulation servicing gaps, not changing reconnection policy.

The new candidate enables C64_1581_NET_SLICES and checks the packet-only hook
after each completed C64 raster line while full-drive D81 is active. The hook
calls net_poll only when at least 2,000 us have elapsed since its previous call.
It is outside Rust's drive borrow and outside CPU instructions. It does not call
frontend_poll, net_frame, or API command execution: media changes and restarts
remain deferred to the safe frame boundary. The existing packet handler can
receive/upload bytes but cannot install the new emulated media at this point.

The interval is a target, not a hard latency guarantee: a long raster line,
media-load operation, radio operation or frame-boundary action can still delay
service. Hook time is included in the outer emulation profile, separately
reported in cumulative c64_net.poll_us. c64_net calls/gaps reset when the drive
is inactive. CRT and other inactive-drive operation have no added service calls.
This first change addresses the saturated full-drive path; it does not claim to
fix the earlier unexplained multi-minute outages or startup paging interruptions.

Preserved: 250 MHz; whole-loop and instruction-tail shortcuts; small idle handler
in SRAM; general dispatcher in flash; Wi-Fi startup gate and Nimbus retry policy.
The earlier unthrottled eight-line polling experiment remains historical; this
candidate uses a time-based cap to bound servicing overhead instead.

Hardware comparison: use 8bitretro.local, disable Nimbus retries after selecting
C64, keep sound/turbo off and load the same D81. Once on the mountain screen,
clear the browser trace, try several controls and collect a 60-second trace plus
two /api/status samples. Compare browser latency/timeouts, c64_net hook gaps and
cost, and frame rate against ~40.4 fps. network.poll_gap_us is a lifetime maximum
and can include boot/loading; c64_net.max_gap_us can include the active loading
period too. A single large historical maximum does not establish ongoing stalls.
Then switch to CRT and verify 50 fps, inactive drive and zero c64_net counters.
Keep network-diagnostics-v2 as the rollback build. Nothing published.

Candidate validation: all three machine builds and combined UF2 payload/bounds checks passed. The actual polling hook passed sanitizer-backed interval, reset and wraparound tests. Wi-Fi gate tests passed and radio source hashes remained unchanged. A 10,000-frame native D81 run completed; all 20 framebuffer captures and 20 drive-RAM captures matched the accepted idle-SRAM build byte for byte. SRAM helper remains 822 bytes; static heap headroom is 32,560 bytes (not live free RAM). Physical board testing remains pending.

### 5 ms comparison build

The 2 ms hardware test sustained about 35.7–36 fps at the mountain screen, versus about 40.5 fps before intra-frame polling. Active hook gap maximum was 6.95 ms; CRT returned to 50 fps with hook counters zero. The next candidate changes only the minimum interval to 5,000 us, retaining the same safe service points and radio policy. Actual-function timing/wraparound sanitizer checks, all machine builds, network formatter checks, UF2 bounds/payload validation and SRAM placement checks passed. Hardware comparison remains pending.

## Hardware baseline accepted 6 November 2026

The experimental baseline retains the 5 ms active-D81 packet-only polling
interval, inline drive bus, guarded whole-loop and instruction-tail shortcuts,
small idle helper in SRAM and dispatcher in flash. Clock remains 250 MHz.
Wi-Fi startup gating and Nimbus build-7 backoff/reconnection fixes are retained.
Use the explicit build options in c64-1581-experiment.md: these experimental
features remain opt-in rather than changing normal release defaults.

Hardware: Pimoroni Presto, Monkey Island v1.2 D81 mountain screen, turbo and
sound off, Nimbus retries disabled after C64 selection, accessed through
8bitretro.local. Settled 5 ms sample: 184 frames / 5.024510 seconds = 36.6 fps.
The 2 ms comparison gave about 35.7–36 fps; the earlier build without additional
intra-frame polling gave about 40.5 fps. These are observed samples, not a
controlled benchmark or a guarantee for every game scene.

The recorded D81 status responses had median 136 ms and 95th percentile 551 ms.
No further recorded timeouts occurred after Nimbus retries were disabled.
Sound and ambient controls completed, including two delayed responses of
2.483 s and 2.420 s; one status response took 2.540 s. Wi-Fi link history and
join count stayed unchanged, with no capacity rejections. Trace windows were
reopened and contain gaps: this does not establish continuous availability.

Switching to CRT restored 250 frames / 5.000015 seconds and 251 frames /
5.019945 seconds (50 fps). Full-drive counters and additional c64_net polling
counters were zero. Free RAM was stable between paired samples; connection
expiry counters increased occasionally while requests continued to complete.

Outstanding issues:
- Occasional multi-second web requests, particularly during bursts of control
  changes. Root cause is not established; 5 ms polling does not resolve them.
- First restart immediately after UF2 flashing can leave a black screen.
  A full power cycle restores operation; subsequent physical resets work.
  This is a post-flash startup issue, not a general warm-reset failure.
- A prior Master outage included a restart. User intervention was uncertain,
  so it is recorded as unexplained, not confirmed automatic recovery or crash.
- Startup/machine-switch and initial Nimbus-attempt timeouts remain observable.
- Full 1581 D81 support remains experimental and read-only; the baseline is
  not a stable release and does not claim original no-poll D81 performance.

Validation before this checkpoint: timing/wraparound sanitizer test for the
actual 5 ms hook; all three machine builds; diagnostic JSON bounds and binary
presence; combined UF2 partition/payload validation; SRAM placement checks.
The preceding 2 ms implementation also passed a 10,000-frame native D81 run
with 20 screen and 20 drive-RAM captures matching the accepted idle-SRAM build.
That game run was not repeated for the interval-only 5 ms change.

## V6 post-reboot interruption, 2026-11-06 19:21 UTC

Reference firmware remains the exact saved startup-diag-v6 UF2. No radio,
network or emulator behaviour was changed for this investigation.

In the supplied 19:21:29–19:22:01 trace, Master was already serving HTTP at
uptime 22,923 ms. Automatic Nimbus reconnect had re-enabled on boot, although
it had been disabled in the preceding D81 session. Source confirms this flag
is initially true and Disconnect changes session state only.

The second failure was recorded at uptime 18,747 ms. The existing 10,000 ms
backoff therefore scheduled attempt three for approximately uptime 28,747 ms
(wall time 19:21:36.070, inferred from received samples). Failure three was
recorded at uptime 33,888 ms: approximately 5,141 ms later. The configured
page timeout is 0x2000 Bluetooth slots = 5.12 seconds; BTstack error 4 denotes
PAGE_TIMEOUT. The browser request started about 19:21:36.550 and timed out at
19:21:40.555, inside that inferred attempt interval. Status was received again
at 19:21:42.827, followed by mostly ~95 ms requests.

During the available samples Wi-Fi link=3, joins=1, transitions=3 and IP changes=1
remained unchanged. The lifetime maximum network poll gap remained 116,516 us,
maximum poll cost 2,107 us; the server recorded no errors, rejected connections
or expirations. This argues against a four-second CPU/network polling stall.
Outgoing Bluetooth paging disrupting Wi-Fi traffic on the shared radio is the
leading explanation for this specific timeout, but the trace does not include
packet captures or an exact attempt-start timestamp, so causality is unproven.
Do not generalise it to all historical web outages.

Next controlled test needs no new firmware: on unchanged v6, let Master run
with the remembered Nimbus switched off and retries enabled; collect a trace
covering several attempts. Then use Disconnect, verify reconnect_enabled=false
(and no ongoing connection attempt), clear the trace and observe for another
2–3 minutes without rebooting. Optionally repeat with Nimbus powered on and
connected. Compare new timeout increments with attempt/failure increments,
not lifetime error totals. Disconnect does not forget the controller; Reconnect
restores the existing policy. Reboot re-enables retries and is not part of the
within-session comparison. Avoid changing paging timeout/backoff until this
comparison, since doing so could regress the tested machine-switch reconnect.

## Shorter outgoing page candidate (Bluetooth build 8)

The follow-up enabled-retry trace showed six browser status timeouts over about
three minutes, tracking retry attempts, with no Wi-Fi rejoin and a constant
55,309 us maximum polling gap. The subsequent same-session disabled-retry trace
covered 179.86 seconds and 106 successful responses with zero timeouts/errors;
attempts stayed at seven. Successful-response medians were 96/97 ms respectively.
This strongly implicates outgoing paging for this particular interruption.

Candidate build 8 changes gap_set_page_timeout from 0x2000 (5.12 s) to 0x1000
(2.56 s). It retains the initial listening window, 3/10/30/60-second failure
backoff, Wi-Fi readiness gate, incoming acceptance and manual controls. Existing
D81 optimisations and 5 ms polling remain. STARTUP_DIAG_PREMAIN is explicitly ON
in all four images, retaining v6 startup instrumentation. The exact saved v6
reference is not overwritten. Status exposes build=8 and page_timeout_ms=2560.

This may shorten shared-radio disruption; it does not guarantee HTTP response
time or successful controller reconnection. Shorter paging can reduce the chance
of finding a controller on a particular attempt, so controller-on hardware tests
are essential. No browser timeout is enlarged to hide missed requests.

Validation: all targets built, reconnect/backoff and Wi-Fi gate tests passed,
5 ms polling passed, runtime/partition placement passed, UF2 payload checks
passed at 11,782 blocks. C64 static headroom remains 32,564 bytes. Hardware pending.

Test cold boot first. With Nimbus off and retries enabled, clear the trace and
capture 3 minutes on Master. Then power Nimbus on and verify connection, controls,
and repeated Master -> C64 -> Model B -> Master switches. Send the trace and note
any need for manual reconnect or a controller power cycle. Retain v6 as fallback.
