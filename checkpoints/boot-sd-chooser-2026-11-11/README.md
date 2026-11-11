Verified four-machine BOOT SD chooser
=====================================

BOOT pauses emulation and temporarily mutes audio while maintaining web and controller service. When a controller is enabled and connected and compatible SD titles exist, BOOT cycles chooser → status → emulator; otherwise status → emulator. Up/down selects titles; Nimbus A or mapped Fire loads the selection.

Status and chooser use compact alternating SRAM images, published during vertical blank. All four machines verified on hardware. Previous checkpoint `reference/boot-status-20261111` and the stable release are preserved.

The manifest records the exact tested UF2 digest and component IDs. Firmware binaries and bundled game data are retained locally, not committed.
