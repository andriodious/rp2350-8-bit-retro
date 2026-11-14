# Classic Bluetooth keyboard — all four machines

BBC Master 128, BBC Model B, Commodore 64 and ZX Spectrum 48K support a Classic HID keyboard alongside the original Nimbus. BLE-only keyboards are not supported. A generic device named Bluetooth Keyboard has been verified; other models may differ.

## Pair and reconnect

Disconnect the keyboard from its Mac/iPad or other host, switch it on and press Connect to enter pairing mode. In the web Bluetooth keyboard pane, Scan for keyboards, select the matching name/address and Connect. Stop scan cancels discovery. Follow any displayed PIN instructions. A connected address is displayed without needing manual address entry.

Pairing is remembered across machine switches and power cycles, independently of Nimbus. Keyboard and joystick startup wait for Wi-Fi readiness. A sleeping keyboard may need a keypress and time to reconnect. Reconnect resumes after Disconnect; Forget removes remembered pairing. Keep the keyboard nearby during discovery.

## Per-machine mappings

Open Key mappings, Add mapping, press one physical Bluetooth key, then click the desired key on the emulator keyboard. Release the physical key. Capture suppresses input until release, can be cancelled, and times out after 60 seconds. Up to 32 overrides per machine are saved; adding an existing source replaces it. Select a mapping and Delete mapping to restore default handling. Reset/joystick controls are not valid mapping destinations. Nimbus Game controls remain separate.

## Troubleshooting and rollback

An empty discovery chooser means no selectable keyboard has been found; ensure pairing mode is active and another host is not connected. The startup and BOOT status screens also report keyboard state. Web typing remains available without a Bluetooth keyboard. Game joystick mode may need enabling separately (including C64 Elite).

The settings format is v2 with whole-record integrity and imports previous v1 settings. Older firmware cannot read v2 settings and rollback may require re-pairing and reapplying preferences. No new save-disc overwrite payload is included in normal combined firmware.
