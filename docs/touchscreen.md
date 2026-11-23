# Touchscreen operation

v0.8.0 supports BBC Master, BBC Micro, C64 and Spectrum without a browser for everyday use. Keyboard and Nimbus navigation remain available alongside touch.

## Open and leave the menus

Tap the running emulator to open its machine keyboard. Physical BOOT opens SD Titles directly. Emulation pauses and sound is muted while menus are open; EXIT resumes cleanly without catching up the paused time. The web interface continues to respond.

On the keyboard, tap a key to return to the emulator and send that keypress. SHIFT toggles shifted legends and input; it starts off each time the keyboard opens. KEYS/MORE KEYS exposes additional keys where provided. RESET resets the emulated machine. SETTINGS opens SD Titles. Cursor keys and Nimbus directions move the highlighted key; Return or A/Fire selects it.

## SD Titles

Tap a title to boot it immediately. PG-UP and PG-DOWN scroll one page of five titles at a time; keyboard or Nimbus Left/Right also change pages. The highlighted row is kept where possible, and paging stops at the first or last page. Keyboard or Nimbus Up/Down select individual files; Return, A/Fire or the on-screen BOOT button loads the highlighted title. KEYBOARD returns to key entry. NEXT opens Configuration; EXIT resumes the existing title. Compatible files are filtered for the current machine.

## Configuration

Tap a machine button to switch and remember it for the next power-on. Selecting the current machine resumes it. Sound and LEDs have on/off buttons and sliders for volume and brightness. LED changes take effect immediately, including while the menu is open. Preferences are saved separately for each machine. PREVIOUS and NEXT move between menu pages; EXIT resumes emulation. Turbo is off by default and remains a web-only control.

## System Status and Bluetooth setup

System Status shows the machine, image, network and input connection status. JOYSTICK SETUP and KEYBOARD SETUP open dedicated device controls. Both support scanning, stopping a scan, selecting a device, connecting, disconnecting, reconnecting and forgetting saved pairing. Follow any displayed pairing/PIN instructions.

Joystick Game controls configure input mode, joystick port, stick/D-pad source, axis inversion, deadzone and button mappings. Keyboard Key mappings captures a physical key, then lets you choose its emulator destination; existing mappings can be deleted. Up to 32 keyboard overrides are saved per machine. BACK returns to the previous setup page, STATUS returns to System Status, and EXIT resumes the emulator. Held menu inputs do not carry into gameplay.

Touch and SD operation need no Wi-Fi or connected input device. In this experimental candidate, Bluetooth also starts without saved Wi-Fi credentials. If Wi-Fi is configured, both Bluetooth devices show **WAITING FOR WIFI** until DHCP succeeds or the full 30-second timeout expires; an early network failure does not shorten the wait. Bluetooth power-up is deferred so incoming connections cannot bypass it. Once offline access is released, Wi-Fi retries do not restart the wait. Sleeping keyboards may need a keypress to wake. This port of the hardware-verified BBC fix awaits four-machine hardware testing; published v0.9.1 is unchanged.

Diagnostics, trace downloads and firmware/media uploads remain available through the web interface; diagnostics are not exposed on the touchscreen. See the [keyboard guide](bluetooth-keyboard.md) for compatibility and mappings.

## Documentation captures

The Master examples in the README are rendered from the firmware drawing code with illustrative device and SD data, rather than photographs of a connected device. Reproduce them with:

```sh
mkdir -p /tmp/retro-menu-previews
MENU_PREVIEW_DIR=/tmp/retro-menu-previews MENU_PREVIEW_IMAGE_ID=2cf31602a06c3aa3 python3 tests/boot_status/test.py
```

The test exports PPM files, which can be converted to PNG. The status example intentionally shows connected Bluetooth devices with Wi-Fi unavailable.
