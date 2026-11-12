# Experimental Classic keyboard mappings checkpoint — 12 November 2026

Pause checkpoint requested by the user, not a published stable release.
Tag: `reference/four-machine-keyboard-mappings-20261112`.
Previous verified Master tag `reference/master-classic-keyboard-20261112` is preserved.

## Firmware

Parent workspace artifact: `artifacts/four-machine-keyboard-mappings-20261112/presto-four-machine-keyboard-mappings.uf2`.
SHA256: `14ce000bfaa911da832112c77b68dca7014e453ecc3556bee645cc8ab7b4ccda`.
Images: Master `70d824fdf660a16d`, BBC `00ca31664b57e011`, C64 `63a6301e66b4428d`, Spectrum `ce517b070eb7b9de`.
Preserve this exact hardware-test binary; rebuilding after this commit changes its source identity.
Build entry point: `sh scripts/build-classic-keyboard-four.sh` with the local toolchain environment.

## Included

Classic keyboard and independent Nimbus connections across four machines; keyboard scanning and selection; matching Bluetooth web panes, connected addresses, contextual Stop scan controls and readable status; Wi-Fi waiting banner; startup/BOOT keyboard status. C64/Spectrum omit unused BBC save buffers. D81 optimisations remain enabled.

Up to 32 Bluetooth keyboard overrides per machine: capture a physical key, click an emulator key, review/delete mappings. Capture suppresses keyboard input, waits for release and times out after 60 seconds. Whole-record settings v2 integrity protects interrupted writes and imports v1 settings. Older firmware cannot read v2 settings; rollback may require re-pairing and restoring preferences.

## Verification so far

Software checks passed: four-target build and UF2 payload/partition verification; C64 static headroom 30,248 bytes; HID parsing, capture/release/timeout, input ownership, mapping command limits and save failures; per-machine persistence, v1 migration and interrupted writes; browser mapping workflow on all four keyboards; status display bounds/restore and Wi-Fi retry gate.

Hardware: user is currently testing Master image `70d824fdf660a16d`. Two supplied status samples show 100% speed, keyboard and Nimbus connected, zero invalid keyboard reports, mapping `[67,32]` present, capture inactive and no pending/error save. User explicitly says testing is still on Master; `system.choice: c64` is not evidence of a failed switch. Earlier pre-mapping C64 build also showed simultaneous connections and full speed.

## Resume tomorrow

Continue Master hands-on mapping/add/delete/default restoration and captured-key suppression checks. Then verify BBC, C64 and Spectrum typing/mappings, machine-switch and power-cycle persistence, independent per-machine mappings, Nimbus controls, both Stop scan buttons, sound/web responsiveness and C64 D81 performance. Do not mark all-machine hardware validation complete yet.

Stable v0.5.0, published assets and prior checkpoint artifacts are unchanged. This checkpoint is local; no push or release publication was requested.
