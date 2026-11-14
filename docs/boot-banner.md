# Power-on banner

Power-on and external reset display the supplied pixel-art logo at the top left,
with white text on black for approximately three seconds before emulation starts.
The page shows the Git build identifier (with `dirty` for local changes), compile
date/time, DHCP address and current joystick and Classic keyboard connection states. It also gives the
web address, `http://8bitretro.local/`.

Wi-Fi and controller connection progress during the banner. With configured Wi-Fi
credentials (including the build secret), it waits for a nonzero DHCP address,
then displays that address for at least another two seconds. Without credentials
it keeps the normal three-second display. A 30-second network timeout allows
offline boot; joystick connection never holds up boot. The banner is always
visible for at least three seconds. Web machine switches and watchdog recovery skip the banner entirely, retaining
the normal network/controller startup path. Recovery paths without
an initialized display safely skip it. This does not change the Master power-on
default or machine-specific controller settings.

The renderer temporarily uses the idle disk-upload staging area in PSRAM and a
small heap-allocated pair of row tables. Status changes are rendered into an
inactive buffer and then displayed; the visible buffer is never cleared or
redrawn. Emulation remains paused, with no emulator cursor drawn. The backlight
is briefly blanked while entering and leaving the banner to hide stale scanlines. It restores the original display table before
releasing the row table. The HTTP listener starts only after the banner has
finished, preventing uploads from colliding with its framebuffer. Bluetooth
bond writes are deferred until normal frame processing, when core 1 is ready
for flash lockout. No emulator frames or save-disc writes run during the banner.

`assets/boot-logo.png` is the user-supplied logo. Its checked-in 8-colour header
is regenerated with `python3 tools/embed_boot_logo.py` (Pillow required only for
regeneration). Firmware builds need no image-processing dependency.

Validation:

```
python3 tests/boot_banner_test.py
```

An optional output filename produces a 480x480 PPM from the actual renderer for
visual inspection. The host test verifies display-row layout and terminators,
framebuffer bounds, three-second display duration, restoration, warm-reboot bypass and inactive-buffer updates.
Hardware validation should cover cold boot with/without Wi-Fi, a paired Nimbus
both on and off, and switching between all three machines after boot.

Hardware testing of the IP-wait banner firmware was reported successful by the
project owner on 4 November 2026. The tested package was
`retro-ip-wait-banner-test.zip`; machine switches bypass the banner.

The subsequent `8bitretro.local` hostname and blue web-theme build also passed
hardware testing, as reported by the project owner on 4 November 2026. The
README screenshot was refreshed from that tested interface.
