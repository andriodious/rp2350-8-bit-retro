# Original SteelSeries Nimbus: controller test 5

This separate experimental build adds web-configurable game controls for BBC
Master, BBC Model B and C64. It has not been pushed to the pending C64 PR.
The first build's scanning, connection, buttons, D-pad and both sticks were
confirmed on the user's hardware. The user also confirmed test 2 game mappings work. Test 3 adds persistent settings and reconnection, which need hardware testing.

## Start here

1. Keep the working firmware as a rollback copy. Flash `presto_nimbus_test5.uf2`.
2. Refresh `http://8bitretro.local/` (or the Presto's IP address). Scroll to **Nimbus
   controller**, put the original Nimbus into pairing mode, scan, wait about
   11 seconds and connect as before.
3. Open **Game controls** and click **Default mappings**, then **Apply controls**.
   Controls start OFF the first time. Apply now saves the settings for this machine.
4. The defaults are joystick mode, left stick plus D-pad, 15% dead zone and
   A as fire. The BBC machines default to joystick 1; the C64 defaults to port 2.
   Games which use the other port need that selected here and applied.
5. Test movement and fire in a game. BBC games may require their own joystick
   option to be enabled. Check both horizontal and vertical movement. If a game's
   stick direction is reversed, change **Invert stick X/Y** and Apply.
6. To use keyboard controls, select **Keyboard**, assign Up/Down/Left/Right and
   the controller buttons to the game's keys, then Apply. Default direction
   mappings are cursor keys. Set A to the game's fire key in this mode:
   **Joystick fire** is only active in joystick mode.
7. You can use the right stick instead of the left, or use the D-pad alone.
   Button key assignments also work in joystick mode, for extra game functions.
   Analog triggers are treated as pressed/released buttons for assignments.
8. Test **Disconnect** while holding a direction/button. The emulator should
   release controller keys, centre analogue inputs and release fire. Keyboard
   keys held independently remain held. Changing mappings also releases old keys.
9. Use **Download controller status** and send the JSON file back if something
   behaves incorrectly, describing the machine, game and selected mode/port.
   Status includes raw inputs, normalized axes and the applied configuration.

The D-pad combines with the selected stick. Opposite digital directions cancel.
The stick dead zone rescales analogue movement outside the centre zone; digital
joystick/keyboard directions engage beyond one quarter of that remaining travel.
Inversion applies to the stick, not the D-pad. Only the selected stick is mapped.

Pair once after installing test 3: test 2 did not save its pairing key. Wait until
status says **Controller remembered**, then restart. With the Nimbus switched on,
it should reconnect without another scan/pair. **Reconnect saved controller**
resumes reconnect attempts after using Disconnect. Disconnect pauses reconnection
until you request it or restart. Forget controller removes the remembered pairing
but retains all three machines' control profiles. Pair again to use that controller.

Apply saves the current machine's profile. Switch to each machine once, choose
its controls and Apply. Subsequent machine switches restore the appropriate
profile and share the same pairing. Games may still require their own joystick
option enabled. Inbound HID connections are accepted only from the saved device;
scan remains limited to eight results and one controller is supported.

Test a full power cycle and switching Master → Model B → C64. Verify each mode,
port and button mapping returns correctly. Check a normal disk save and confirm
Wi-Fi still connects. Download status if reconnect fails: it includes remembered,
save_pending and save_error flags, but never the pairing key.

## Build

From this source checkout, after the normal setup:

```sh
scripts/setup.sh --no-build --roms
. deps/env.sh
OS_CMAKE_ARGS="-DSOUND=ON -DAMBIENT=ON -DWEBDIAG=OFF -DNIMBUS_TEST=ON" \
  scripts/build-dual-boot.sh build-nimbus-test5
```

Output: `build-nimbus-test5/presto_dual_boot.uf2`. Use a new build directory when
changing options. `NIMBUS_TEST` defaults OFF. Existing performance options remain
ON and the clock remains 250 MHz. The regular release is unchanged when disabled.

## Implementation

`presto_bluetooth.c` shares Wi-Fi's polled main-core async context. BTstack uses its RAM database while running; a wrapper saves changed pairing keys
through the project's own persistence module, not the SDK flash TLV backend. HID logical ranges
normalize stick values; original Nimbus Y axes are inverted to screen coordinates.
Malformed/truncated fields are rejected before parsing. `controller.c` applies
inputs from `frontend_frame`, combines controller/web/USB keyboard ownership,
and handles configuration without dynamically allocating memory.

`controller_store.c` stores a versioned CRC32 record in redundant sectors at
0x00F00000 and 0x00F01000, immediately after the application slot. These are named
regions in the existing flash layout; no saved-disk or Wi-Fi regions move. Records
contain one bond and three control profiles. A changed record is written to the
other bank and read back for verification; boot selects the newest valid record.
An interrupted write leaves the old bank available. If both copies are invalid,
the controller starts unpaired with controls disabled. Duplicate updates do not
write flash; movements and status polling never write flash. Failed writes are
reported and are not retried continuously. Apply can explicitly retry.

Bluetooth callbacks only update the SRAM record. `presto_bluetooth_frame` commits
it from the frontend frame boundary using `mem_flash_write_sector`, which parks
core 1 and preserves QMI timing. Configuration commands use the same safe writer
from the existing API's between-frame command handling. Source compile-time guards
and linked-layout tests check separation from both BBC save areas and app/firmware
partitions, including the configured save shift. The UF2 contains no saved keys.
Older test builds ignore these sectors; reflashing test 3 restores their data.

Master/Model B use ADC channels 0/1 or 2/3 and active-low system VIA PB4/PB5 fire.
The Model B masked VIA setter preserves the other peripheral input bits. C64 uses
active-low joystick bits through the existing port 1 keyboard-poll and port 2
input hooks, retaining the web keyboard's joystick controls on port 2.

`POST /api/bluetooth` accepts `scan`, `connect <index>`, `disconnect`, `reconnect`, `forget`, or
`config <19 integers>`. Configuration is mode (0 off, 1 joystick, 2 keyboard),
port (1/2), movement (0 left, 1 right, 2 D-pad), dead zone (0–50), invert X/Y
(0/1), followed by 13 assignments: Up, Down, Left, Right, A, B, X, Y, L1, R1,
L2, R2, Menu. Assignments are -1 none, -2 joystick fire or 0–127 machine keyboard
codes. All values are validated before replacing the configuration. Status
returns the same array under `bluetooth.config`.

## Validation

All three machine variants compile. Local address/undefined-behaviour sanitizer
tests cover normalization, ports, dead zone, inversion, opposite directions,
multiple buttons assigned to one key, keyboard ownership, reset, disconnect,
and invalid configuration. Persistence tests cover per-machine reload, duplicate-write suppression, corrupt banks, interrupted writes and forgetting a bond. Browser tests cover all three machine forms, requests,
default mappings, preserving edits during polling and status download. Packaging
checks compare every combined UF2 payload to its source target and check that
no Bluetooth flash-key backend is linked. These are not hardware game tests.

Run the native mapping checks with:

```sh
cc -DNIMBUS_TEST=1 -Wall -Wextra -Werror -fsanitize=address,undefined \
  -Isrc/common src/common/controller.c tests/bluetooth/mapping_test.c \
  -o /tmp/nimbus-mapping-test
/tmp/nimbus-mapping-test
```

The original Nimbus Classic protocol and usage mappings were checked against
[Bluepad32](https://github.com/ricardoquesada/bluepad32) and Ricardo Quesada's
Apache-2.0 Nimbus parser; no Bluepad32 source is compiled into this build.
BTstack is by BlueKitchen; its license and the Pico SDK license are included in
`docs/licenses`. Existing emulator and Elite credits remain in `c64-credits.md`.

Test 3 linked results: C64 static headroom 41,276 bytes (before runtime heap use); 10,956 combined UF2 payload blocks verified. Saving a changed configuration or new bond can briefly pause emulation while the safe flash writer runs.


## Test 4: reconnect timing

Existing test 3 pairing and profiles are retained; no new pairing is normally
needed. Flash test 4, refresh the browser, switch machines and leave the Nimbus
searching. It first listens for three seconds before initiating an outgoing HID
connection. Failed or closed connections get another listening window. BTstack
rejects incoming HID control connections while its outgoing HID connection object
exists, so immediate repeated outgoing attempts can interfere with the controller
reaching its host. The page timeout is now 5.12 seconds rather than ~15 seconds;
this limits the radio paging stage, not every subsequent protocol exchange.

Reconnect gives immediate acknowledgment. One click enables reconnect; additional
clicks do not restart an in-progress attempt or push the retry deadline back.
Status shows listening/connecting/connected. Downloads include build 4, retry_ms,
attempts, incoming and failures. If the delay persists, download status while it
is stuck and again after connection succeeds, and report the approximate delay.
The timing-collision explanation is based on source behaviour, not a captured
radio trace; this build needs hardware verification.

All three targets compile; mapping/persistence behaviour is unchanged. Reconnect
policy tests cover repeated clicks, listening deadlines, busy states and clock
wrap. Browser tests check the feedback. Final UF2 verification matches 10,966
payload blocks; C64 static headroom is 41,264 bytes before runtime allocations.


## Test 5: clean Bluetooth shutdown on web machine switches

The supplied C64 snapshot was taken after reconnection: three outgoing attempts,
two failures, one incoming connection, descriptor ready and one input report.
It is a single snapshot, not a history of the preceding runs. It does not reveal
the earlier failure codes because test 4 overwrote error on success.

Web-requested restarts now ask BTstack to power down gracefully, which sends HCI
disconnect commands for open links. The existing network poll runs for up to
1.5 seconds waiting for OFF; the watchdog is serviced and the restart proceeds
even if cleanup cannot finish. Reconnect is disabled during shutdown, controller
inputs are released, and pending settings are flushed using the existing safe
writer. This does not change the behaviour of a physical reset or power removal.

This addresses a plausible stale connection at the controller after an abrupt
machine switch; it is not yet confirmed as the cause. Existing pairing/settings
remain compatible. Test repeated Master → BBC → C64 → Master → C64 switches
without turning the Nimbus off. If it sticks searching, download status before
power-cycling the controller, then again after recovery.

Status includes last_failure, last_failure_phase and last_failure_ms; these
survive successful reconnection within the same firmware run. Phases: 1 immediate
outgoing-request error, 2 connection-open failure, 3 HID-descriptor failure.
They reset when the Presto reboots; no extra flash writes are made for logging.
All three builds pass; combined image matches 10,973 payload blocks. C64 static
headroom is 41,252 bytes before runtime allocations.
