# Original SteelSeries Nimbus: Bluetooth test 1

This is a separate experimental build, not an update to PR #10. It adds a
single Bluetooth Classic HID connection and web input diagnostics to Master,
Model B and C64. It does not yet send joystick events to the emulators.

## Try it

1. Keep your working optimised UF2 as a rollback copy, then flash the supplied
   `presto_nimbus_test1.uf2` using the same procedure as before.
2. Open `http://beeb.local/` (or the Presto's IP address) and refresh the page.
   Scroll down to **Nimbus Bluetooth test**. Wi-Fi credentials must already be saved.
3. Disconnect the original Nimbus from other hosts and put it into pairing mode.
4. Click **Scan for controllers**. Allow about 11 seconds for the scan to finish.
   The state should change from `scanning` to `idle`.
5. Select the Nimbus entry and click **Connect**. Names appear only when included
   in the Bluetooth inquiry response; some devices may show just an address.
   If uncertain which address is the controller, compare scans with it off/on.
6. Look for `connected`, `descriptor: true`, and an increasing `reports` count.
   Move each stick, hold each direction and button for at least two seconds, and
   check the displayed values. The page samples status at its usual interval;
   a brief tap may occur between samples. Values are raw HID values, not calibrated.
   Axes are X/Y/Z/Rz; buttons are HID usages 1–8 (A/B/X/Y/shoulders/triggers on
   the original Nimbus). Hat and separate D-pad usages are shown separately.
7. Click **Download controller status** and send the JSON file back. If pairing
   fails, download the status anyway: it includes the Bluetooth error code,
   memory readings and the machine's status.
8. Check that the web interface remains responsive while connected, then repeat
   in the other two machine modes. Switching machines restarts the Bluetooth host.

Pairing keys exist only in RAM. Repeat pairing after restarting/switching machine;
if the controller retains its old pairing, put it back in pairing mode. No
Bluetooth keys are written to flash. Only explicitly selected outgoing devices
are accepted. Scan is limited to eight results and one connection is supported.

## Build

From this source checkout, after the normal setup:

```sh
scripts/setup.sh --no-build --roms
. deps/env.sh
OS_CMAKE_ARGS="-DSOUND=ON -DAMBIENT=ON -DWEBDIAG=OFF -DNIMBUS_TEST=ON" \
  scripts/build-dual-boot.sh build-nimbus-test
```

The result is `build-nimbus-test/presto_dual_boot.uf2`. Use a new build directory
when changing options. `NIMBUS_TEST` defaults OFF. Existing CPU/VIC SRAM,
packed-pixel and render-overlap defaults remain ON; clock stays 250 MHz.

## Implementation and verification

`presto_bluetooth.c` uses the SDK's BTstack Classic HID host, report-descriptor
parser, CYW43 transport and polled async context shared with Wi-Fi. Its SDK init
hook uses BTstack's RAM link-key database, avoiding the SDK's default flash TLV
backend and its unrelated storage layout. Bluetooth storage pools are bounded.
The web API command is `/api/bluetooth`, with `scan`, `connect <index>` or
`disconnect` as its POST body. State is included in `/api/status`.

All three targets compile and the combined UF2 is checked against their payloads.
Browser tests exercise commands, status download and hiding the panel when the
feature is absent. These checks do not establish on-hardware pairing or radio
coexistence. Hardware testing of this first build remains outstanding.

The original Nimbus uses Bluetooth Classic, as documented by Bluepad32:
https://github.com/ricardoquesada/bluepad32/blob/main/docs/supported_gamepads.md
Its usage mappings were checked against Ricardo Quesada's Apache-2.0 Nimbus
parser; no Bluepad32 source is compiled into this build. BTstack is by BlueKitchen;
its license is included with the test distribution. Existing emulator, SDK and
Elite credits remain applicable (see `c64-credits.md`).

Local build results: C64 static headroom is 42,160 bytes with this test enabled,
versus 48,016 bytes with it disabled (5,856 bytes additional static SRAM).
The disabled build has no Bluetooth host symbols. Combined image verification
matched all 10,894 payload blocks to the launcher and machine binaries and found
no Bluetooth flash-storage backend in any target. Existing SDK/newlib compiler
warnings remain; compilation and linking completed successfully.
