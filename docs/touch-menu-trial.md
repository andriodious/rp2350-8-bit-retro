# Touch menu trial

Tap the display or press BOOT to open the SD chooser, without requiring a joystick
or keyboard. Empty cards show an empty list with navigation available. Tap a title
to load it immediately, or use Up/Down then the on-screen BOOT button. Up and Down move the selection and
page through longer lists.

The outlined bottom Previous, Next and red Exit buttons navigate screens. The sequence is
SD chooser, configuration, status, then emulator. BOOT advances as before.
Configuration provides machine buttons, sound and LED toggles, and draggable
volume and ambient-brightness sliders. Selecting the current machine resumes it.
Keyboard Up/Down and Return and Nimbus navigation/selection remain available.
Audio stays muted while menus are open; settings use the existing saved preference
commands. This experimental build was hardware verified on all four machines on 22 November
2026; it is not a published release.

Touch hardware follows Pimoroni Presto's FT6236 driver: I2C1, GP30/31, interrupt
GP32, address 0x48. Transactions have bounded timeouts and error retry backoff.

## Verified checkpoint — 22 November 2026

The user reported all checks passed: all four machines, touch navigation and
selection, keyboard and Nimbus controls, menu exit and resume, remembered-machine
power-on, and stable startup screens. The SD action is labelled BOOT; page titles
are bold and centred, with centred status footer text and a MACHINE: label.
Startup scanout uses immutable SRAM rows and restores the borrowed framebuffer.

The exact tested combined UF2 is retained without rebuilding:
`retro8bit-four-machine-startup-screen-test.uf2`.

SHA-256: `0f099351ebd54a0716fe204d6942186e5fd0f7b424a0cdf2ae22b1d224c08fe6`.

| Machine | Image ID |
| --- | --- |
| Master | c498952963d9be9b |
| BBC Micro | c88d2ee0ce79af35 |
| C64 | 5b3ff7cee21f36e3 |
| Spectrum | 7de6b8974326075f |

Launcher SHA-256: `4f64f88f7b5f22f9d7f0409459142639f6a7045db5d71802d2757f2f14d868fa`.

Host checks passed for all four machines, including startup framebuffer restoration,
menu navigation, row encoding and bounds. Combined UF2 payload verification passed;
C64 static SRAM headroom is 30,192 bytes. Previous checkpoints and v0.7.0 are unchanged.


## Bluetooth setup candidate (22 November 2026)

The paused System Status screen now offers Joystick Setup and Keyboard Setup.
Both use the same Classic pairing, reconnect, forget and persistent mapping
commands as the web controls. Device discovery has Scan, Stop Scan, paginated
name/address results and an explicit Connect action. Keyboard pairing PINs appear
on-screen. Forget asks for confirmation. No diagnostic export or raw-input view
is exposed on the touchscreen; those remain web-only.

Joystick Game Controls edits mode, port, stick/D-pad source, dead zone and axis
inversion. Button Mappings covers all thirteen directions/buttons, including
None and Fire targets. Defaults changes the draft; Apply saves it for this machine.
Keyboard Key Mappings lists saved pairs, captures a Bluetooth key for Add, then
shows a paginated machine-key picker; Delete removes the selected pair. Capture
cancels on Back, Status, Exit or BOOT, and times out after sixty seconds. Captured
keys are suppressed until released before normal menu navigation resumes.

Touch and keyboard/Nimbus Up/Down plus Return/Fire operate the setup controls.
BACK returns to the parent screen, STATUS returns to System Status, and EXIT
resumes the emulator. BOOT from a setup page returns to System Status.
The previous tap-file-to-boot improvement is included.

Host tests cover all four framebuffer formats, every setup page within the
immutable SRAM scanout bank, device selection and scan guards, capture/cancel,
map/delete commands, draft/apply settings and held-button navigation. Bluetooth
parser, mapping and discovery regressions also pass. This is a test candidate,
not hardware verified. The verified touch-menu tag and v0.7.0 remain unchanged.

## Touchscreen keyboard candidate (22 November 2026)

Touching the running emulator opens the keyboard directly. Physical BOOT opens
SD Titles as before, with KEYBOARD in place of PREVIOUS. The keyboard uses the current
machine's key matrix, with KEYS/MORE KEYS pages for the BBC machines and C64,
and all forty Spectrum keys on one page. Touch a key, or use keyboard/Nimbus
arrows and Return/A/Fire. Selecting a key resumes emulation and queues one press
and release through the same frame-timed input path as the web keyboard.

SHIFT OFF/ON changes the legends and applies Shift to the selected key. Spectrum
also has a Symbol Shift toggle. RESET queues the standard emulator reset; SETTINGS
(or BOOT) returns to SD Titles without delivering a key. Each opening starts
with Shift off. The emulation remains paused and muted until a key or Reset is
selected, or the menus are exited. Touch and held selection inputs are guarded
against accidental repeat actions.

This combined test candidate includes the Bluetooth setup screens above. Host
checks exercise all four native framebuffer formats, shifted pages, scanout
bank bounds, directional input, single-key delivery and clean resume. It still
requires hardware testing; the verified checkpoint and v0.7.0 are unchanged.

Keyboard visual-state follow-up: SHIFT OFF uses a black background and white
text; SHIFT ON uses cyan and black. KEYS/MORE KEYS fill reflects the active
page. Navigation focus on an inactive toggle/page uses a thin cyan outline,
without changing its off-state fill. Native pixel-colour checks pass on all
four machines.

Master resume pacing fix: time spent in the paused frontend menu is excluded
from the Master's 2 MHz pacing epoch and speed-statistics window. Previously
that elapsed time became catch-up CPU work on return, causing a temporary
speed-up despite Turbo being off. Host regression tests exercise long and
repeated pauses while retaining network/input polling. Hardware confirmation
with Arcadians is pending.

Live LED settings candidate: on/off and brightness request a core-1 refresh
independently of emulator frames, including the C64 and Spectrum idle workers.
Paused-menu updates re-scale cached game colours rather than sampling the menu.
A busy LED FIFO leaves the request pending for retry. Existing lock indicators
and minimum ambient glow retain their normal behaviour. Four-format driver
tests cover updates without frames, off/on, FIFO retry and resumed sampling.
The Master resume pacing fix remains included; hardware confirmation is pending.

## BBC Micro menu allocation correction (2026-11-22)

Hardware report on image `9ef942089611f1ed`: BBC Micro runs at full speed but BOOT and touch do not open menus; free SRAM is 48,688 bytes. The old menu allocation requires 46,848 bytes of pixels plus 3,848 bytes of row pointers, exceeding that total before allocator overhead. The startup banner also silently skips when its larger allocation fails.

The candidate borrows BBC Micro's existing SRAM framebuffer after synchronising the render wire, saves the overwritten pixels in reserved PSRAM and restores them before resuming. Only the row pointers need new SRAM. Packed startup rows now use 96 entries per bank instead of 128, saving 15,616 bytes on BBC machines. Tests render the real build-label-length banner and reject large BBC Micro allocations. Watchdog restarts still deliberately skip the startup banner; verify it after a full power cycle.

Menu rendering for all four machines, pause pacing, live LED updates and combined firmware payload checks pass. Hardware retest is pending. The verified checkpoint and v0.7.0 are unchanged.

## Verified touchscreen keyboard and setup checkpoint — 22 November 2026

User hardware verification: all four machines passed. The startup banner appears,
BBC Micro BOOT and touch menus work, Master resumes normally, and LED settings
update immediately while menus are open.

This checkpoint includes touchscreen keyboard entry and Shift/page highlighting,
Classic joystick and keyboard setup screens, the Master pause/resume correction,
live LED settings, and the BBC Micro menu/startup memory correction. Diagnostics
remain web-only.

Reference tag: `reference/four-machine-touch-keyboard-20261122`.
The exact tested UF2 is preserved without rebuilding:
`retro8bit-four-machine-beeb-menu-memory-test.uf2`.
SHA-256: `de5b7324ae013619ff8669f0a9cc17869a46c2987b8e53f39a3542aa8cdd5701`.

| Machine | Image ID |
| --- | --- |
| master | 915271af19ded98c |
| beeb | b553d59cd75f4660 |
| c64 | aafbed80655a43e7 |
| spectrum | 4d7122c6958a01e8 |

Previous reference tags and artifacts are preserved. Published v0.7.0 is unchanged.
