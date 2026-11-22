# RP2350 8-bit Retro

A multi-machine emulator platform for **Pimoroni Presto**, bringing the BBC Model B,
BBC Master 128, Commodore 64 and ZX Spectrum 48K together on one RP2350 board.

This project continues [8bitkick/rp2350-bbc-micro](https://github.com/8bitkick/rp2350-bbc-micro)
as a broader emulator platform. The original project remains focused on its BBC
experiment. Original Git history, source notices and licence files are retained.

## Download and try it

**[Download v0.9.1 firmware for Pimoroni Presto](https://github.com/andriodious/rp2350-8-bit-retro/releases/download/v0.9.1/rp2350-8-bit-retro-presto.uf2)** — no compilation required.

See the **[v0.9.1 release and first-time setup instructions](https://github.com/andriodious/rp2350-8-bit-retro/releases/tag/v0.9.1)** for flashing, Wi-Fi setup, source downloads and checksums. After flashing and power cycling, tap the screen to use the machine keyboard or press BOOT to browse SD titles. The web interface at **http://8bitretro.local/** remains optional. Existing Wi-Fi settings are retained.

## Machines and features

| Machine | Emulator foundation | Media support | Controller support |
|---|---|---|---|
| BBC Model B | beebjit, including the upstream RP2350 JIT work | BBC disk loading through SD/web | Analogue joystick/fire or keyboard mappings |
| BBC Master 128 | Kieran Connell's kc-beeb integration | BBC disk loading through SD/web | Analogue joystick/fire or keyboard mappings |
| Commodore 64 PAL | FRANK C64 / Frodo | D64/D81, T64/P00 programs and supported CRT cartridges through SD/web | Joystick port 1/2 or keyboard mappings |
| ZX Spectrum 48K | Pinned [chips fork](https://github.com/andriodious/chips) | SNA/Z80 snapshots and TAP/TZX tapes through SD/web | Kempston joystick/fire or keyboard mappings |

- Operate all four machines directly from the touchscreen: enter keys, load SD titles, switch machines, adjust sound and LEDs, and configure Bluetooth devices. No browser, keyboard or joystick is required for menu selection.
- Keyboard and Nimbus menu controls still work; the web interface remains available for uploads, remote controls and diagnostics.
- Classic Bluetooth keyboard support on all four machines, with scanning, remembered pairing, automatic reconnection and up to 32 custom key mappings per machine.
- Sound output, volume controls and ambient LEDs on supported builds.
- Original SteelSeries Nimbus support: pairing, saved per-machine mappings,
  dead zone/inversion controls and reconnection across machine switches.
- Tested C64 optimisations: CPU/VIC code in SRAM, packed pixel conversion and
  overlapping emulation/display conversion. The RP2350 clock remains 250 MHz.

Version **v0.9.1** preserves fast flash reads after saving settings, fixing the persistent C64 D81 slowdown and distorted sound that could follow a menu volume change. It also improves C64 audio-clock accuracy to reduce underruns. The exact clean build passed the requested four-machine hardware checks; all v0.9.0 stability fixes are retained.

Version **v0.9.0** fixes network timer exhaustion that could restart C64, extends startup recovery and corrects display DMA restart ordering. Repeated physical resets from C64 Elite and Monkey Island passed; all four machines passed startup, display, menus, sound and web checks. Its release download remains the exact hardware-tested firmware, with a separate clean source build and host regression checks also completed.

Version **v0.8.0** makes Presto usable independently of the web interface. Touch opens a full machine keyboard; SETTINGS opens SD titles, machine/audio/LED configuration and system status, with dedicated joystick and keyboard setup screens. Wi-Fi failure no longer leaves the keyboard waiting indefinitely. All four machines passed hardware testing, including offline and restored Wi-Fi, both Bluetooth devices and web responsiveness.

Version **v0.7.0** adds Bluetooth keyboard SD selection and a BOOT machine selector. Use Up/Down and Return, or Nimbus Up/Down and A/Fire, to load a title or switch machines without the web page. The exact clean GitHub build was hardware-verified on all four machines, including BOOT cycling, keyboard/Nimbus reconnection, sound and web responsiveness.

Version **v0.6.0** adds Classic Bluetooth keyboards alongside Nimbus, saved per-machine keyboard mappings, matching connection/scan controls, keyboard status on the startup/status screens, and C64 web function-key highlighting. All four machines were hardware-tested with reconnection, saved mappings, sound and web responsiveness.

Version **v0.5.0** adds the ZX Spectrum 48K, verified at 50 fps with Elite, with
beeper audio, a grey-key web keyboard on a black rainbow-striped case, and
Kempston controls. Reset returns to BASIC; Restart Elite loads the built-in snapshot.
Elite, Manic Miner TZX and YU_SKOOL TAP have been tested on hardware. This is a
48K machine; 128K snapshots and every possible TZX encoding are not supported.

### Touchscreen and BOOT controls

- **Tap the running emulator** to open its keyboard. Tap a key to resume and send that keypress. **SHIFT OFF/ON** changes the legends and the next keypress; **KEYS / MORE KEYS** opens additional keys where available. **RESET** resets the emulator; **SETTINGS** opens SD Titles.
- **Press physical BOOT** to open SD Titles directly. Tap a filename to load it, or use **UP**, **DOWN** and **BOOT**. The **KEYBOARD** button returns to key entry.
- **NEXT** opens Configuration: choose a machine, toggle sound or LEDs, and adjust volume or brightness. LED changes apply immediately. The selected machine is remembered for the next power-on; selecting the current machine resumes it.
- **NEXT** again opens System Status, including **JOYSTICK SETUP** and **KEYBOARD SETUP**. Pair, reconnect or forget devices, configure game controls and add/delete keyboard mappings on the Presto itself. Diagnostics remain web-only.
- **PREVIOUS / NEXT / EXIT** navigate the settings screens. Repeated physical BOOT presses cycle **SD Titles → Configuration → System Status → emulator**.

Keyboard arrows/Return and Nimbus directions/A or mapped Fire remain available.
Emulation pauses and audio is temporarily muted in menus; the web interface stays
responsive. Closing a menu resumes at normal speed without carrying menu keys into
the game. Sound, volume, LEDs, brightness and input mappings are saved per machine.
See the [touchscreen guide](docs/touchscreen.md).

Version v0.3.0 adds read-only C64 `.t64` loading from SD or web upload, with a program selector for multi-entry archives. See [T64 support and limitations](docs/c64-t64.md).

Current source also supports P00 programs and standard 8/16 KiB and EasyFlash CRT cartridges, with SD/web loading and cartridge eject. Tested on Presto with Commando P00 and Monkey Island EasyFlash. EasyFlash saves are not supported. See [cartridge and P00 support](docs/c64-cartridge-p00.md).

Pimoroni Presto is the first supported board. Nimbus validation is for the
**original Nimbus**, not Nimbus+ or arbitrary Bluetooth gamepads. Compatibility
with every game or disk fastloader is not guaranteed; for the supplied C64 Elite
image, choose **N** when asked to use its fastloader.

<img src="presto-nimbus-elite.jpeg" alt="Pimoroni Presto running Elite with an original SteelSeries Nimbus controller" width="480">

### Web keyboards

**Commodore 64** — front-face graphics, function keys and joystick controls.

![Current C64 on-screen keyboard](c64-web-interface.png)

**ZX Spectrum 48K** — grey keys, rainbow case stripe and Kempston controls.

![Current Spectrum 48K on-screen keyboard](spectrum-web-keyboard.png)

### Touchscreen controls — BBC Master examples

Captures from the firmware's screen renderer, with example SD filenames and
connection states. These are the same layouts shown on the Presto; the status
example demonstrates Bluetooth connected while Wi-Fi is unavailable.

| Keyboard input | SD game selection |
|---|---|
| ![Master touchscreen keyboard with Shift and Settings](docs/images/master-touch-keyboard.png) | ![Master SD game selector with Boot and navigation buttons](docs/images/master-touch-sd-titles.png) |

| Machine, sound and LED settings | System status and Bluetooth setup |
|---|---|
| ![Touchscreen configuration with four machines and volume and brightness sliders](docs/images/master-touch-configuration.png) | ![Master system status with joystick and keyboard setup buttons](docs/images/master-touch-status.png) |


## Gameplay videos

Recorded on Pimoroni Presto, with sound.

### Elite — Commodore 64

https://github.com/user-attachments/assets/acca71d1-84e7-4a5f-bc91-ceeebc5a8c98

### Ghosts ’n Goblins — Commodore 64

https://github.com/user-attachments/assets/c2f20f99-e762-4cea-ad45-c583d763a71b

### Repton 3 — BBC Master

https://github.com/user-attachments/assets/648c8668-27c7-495a-b540-f22d90917808

## Build on a Raspberry Pi or Linux computer

Start in the extracted release source folder or clone this repository:

```sh
git clone --branch v0.9.1 https://github.com/andriodious/rp2350-8-bit-retro.git
cd rp2350-8-bit-retro
```

```sh
scripts/setup.sh --no-build --roms
. deps/env.sh
EXTRA_OS_CMAKE_ARGS='-DC64_ABORT_DIAG=ON -DC64_RUNTIME_DIAG=ON' scripts/build-classic-keyboard-four.sh build-release
```

On Debian/Raspberry Pi OS, add `--apt` to the setup command if host packages need
installing. Setup downloads the pinned compiler/SDK, BBC assets and dependencies;
C64 configuration fetches the pinned FRANK fork and checksum-verified Elite image.
Spectrum uses the pinned chips fork; its Elite snapshot is downloaded and checksum-verified during configuration, rather than stored in Git.
The source archive contains no downloaded commercial game images, commercial ROM set or Wi-Fi secrets. The licensed OpenSE ROM and synthetic tape test fixtures remain included.

Result: **`build-release/rp2350-8-bit-retro-presto.uf2`** containing the launcher and
all four machines. Sound, ambient LEDs, Classic Bluetooth keyboard and original Nimbus support are enabled;
general profiling and web diagnostics are disabled. C64 terminal-caller capture and the read-only runtime diagnostic endpoint are enabled, matching the tested release configuration. Source rebuilds are separate artifacts and are not automatically hardware verified. See [build instructions](docs/build-retro.md)
for options and an explanation of the legacy build entry points.

## Install and use

1. Hold the Presto's BOOT button, tap RESET and release BOOT to expose its USB drive.
2. Copy `rp2350-8-bit-retro-presto.uf2` to that drive.
3. Power cycle. Tap the display for key entry, or press BOOT to load a title from SD and open settings.
4. Wi-Fi is optional for local operation. For web access, follow [the Presto network guide](docs/presto/wifi.md), then open `http://8bitretro.local/` or the startup IP address. Existing credentials are retained. Bluetooth currently also needs Wi-Fi credentials saved once to initialise the radio; the network itself may be unavailable. Local touch and SD menus need no Wi-Fi configuration.

After flashing this version, use `8bitretro.local` instead of the previous
`beeb.local` or interim `retro8bit.local` address. Saved Wi-Fi credentials are retained.
The tested flash layout and machine identifiers remain compatible. Normal combined
builds do not include save-disk overwrite payloads. Saved Nimbus pairing and
profiles from test 3 onwards are retained. Refresh the browser after updating.

## Controller setup

On the Presto, open **SETTINGS → NEXT → NEXT → JOYSTICK SETUP**, or use the web **Nimbus controller** pane, and pair the original Nimbus once. Under **Game controls**,
choose Default mappings or configure your own, then Apply controls. Each machine
saves its own configuration. Defaults use left stick/D-pad and A for fire, BBC
joystick 1 or C64 port 2. Games may need their own joystick option enabled.

Reconnect is automatic for the remembered controller; one click on Reconnect
resumes it after an explicit Disconnect. Forget removes pairing but keeps mappings.
Machine switches cleanly close Bluetooth before restarting. See the detailed
[Nimbus notes](docs/nimbus-test.md) for controls, storage and diagnostics.

## Bluetooth keyboard setup

1. Disconnect the keyboard from other computers/tablets and put it in pairing mode (usually its Connect button).
2. On the Presto, open **SETTINGS → NEXT → NEXT → KEYBOARD SETUP**, or use the web **Bluetooth keyboard** pane. Scan for keyboards, choose the matching name or address, then **Connect**. **Stop scan** ends discovery early. If a pairing PIN is shown, follow the displayed instructions.
3. The pairing is remembered. The keyboard and Nimbus reconnect independently after a power cycle or machine switch. A sleeping keyboard may need a keypress to wake before reconnecting. **Reconnect** resumes after an explicit Disconnect; **Forget** removes the remembered pairing.
4. Under **Key mappings**, choose **Add mapping**, press a key on the Bluetooth keyboard, then choose its destination from the touchscreen key list or web emulator keyboard. Release the physical key after mapping. Choose a saved mapping and **Delete mapping** to restore that key's normal behaviour.

Mappings are saved separately for Master, BBC Model B, C64 and Spectrum. Up to 32
overrides are supported per machine; unmapped keys retain their default mapping.
Capture temporarily suppresses keyboard input and expires after 60 seconds;
Reset and joystick buttons are not mapping destinations.

This release supports **Classic Bluetooth HID keyboards**, not BLE-only keyboards.
A generic keyboard advertised as “Bluetooth Keyboard” was tested alongside an
original Nimbus. See [keyboard support and troubleshooting](docs/bluetooth-keyboard.md).

Settings are upgraded to integrity-checked v2 records. Older firmware may not read
these records, so rolling back can require re-pairing devices and restoring settings.

## Status and contributions

The v0.9.1 download is the exact clean GitHub build subsequently accepted on
hardware, from source commit `481b5bf6ce9661b9a7a87664fe3a26fd57a579ca` and
`reference/four-machine-c64-flash-read-restore-20261122`. Its SHA256 is
`8ccb9e0a0aaf64e57b846d5352a5a847481d8faae7f0f4dad940e451bd99b108`.
The user confirmed “Build checks out fine” after the requested four-machine
startup, settings-save, menu-resume and sound checks, plus C64 Monkey Island D81
playback and physical resets. Individual counts were not supplied. See the
[clean-build verification record](checkpoints/four-machine-c64-flash-read-clean-verified-20261122/verification.json).
The release adds documentation and build-recipe updates without rebuilding that
firmware. Future source rebuilds are not automatically hardware verified.
v0.9.0 and all earlier checkpoints remain unchanged. Compatibility with every
game, Bluetooth keyboard or controller is not implied.

See [release notes](docs/releases/v0.9.1.md), [credits](CREDITS.md), and
[contribution guidance](CONTRIBUTING.md). The inherited [upstream README](UPSTREAM-README.md)
and technical documents remain available as historical and engineering references.

## Licences and assets

`LICENSE`, `COPYING`, component licence files and source headers are retained.
Different components carry their own terms; this project does not relicense them.
ROMs and Elite are separate copyrighted assets. Fetching or crediting them does
not change their ownership or grant new redistribution rights. Review the relevant
upstream notices before distributing firmware with embedded assets.

### Startup banner

A power-on screen shows the build, DHCP address, joystick and keyboard status. With Wi-Fi
configured it waits for an address, then holds it for two more seconds (30-second
network timeout); otherwise it shows for three seconds.
Machine switching skips the banner. See [startup banner details](docs/boot-banner.md).
