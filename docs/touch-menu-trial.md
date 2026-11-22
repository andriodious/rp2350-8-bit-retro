# Touch menu trial

Tap the display or press BOOT to open the SD chooser, without requiring a joystick
or keyboard. Empty cards show an empty list with navigation available. Tap a title
to highlight it, then tap the on-screen BOOT button to load. Up and Down move the selection and
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
