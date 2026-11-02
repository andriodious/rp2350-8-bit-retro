# Clean Nimbus release

This release uses the hardware-tested test 5 reconnect, pairing and controller
mapping behaviour. It is rebuilt in a fresh directory with sound and ambient LEDs
enabled, all four tested C64 optimisations enabled, profiling OFF and WEBDIAG OFF.
The raw controller readout and status-download button are hidden in the clean
interface. Pairing, connection status, reconnect/forget and game settings remain.
The diagnostic API fields are retained for troubleshooting. Set WEBDIAG=ON to
show the diagnostic web controls again.

Flash `presto_nimbus_clean.uf2` as usual, then refresh the browser. Existing test
3–5 saved pairing and per-machine mappings remain compatible. No re-pairing is
normally needed. The package includes test 5 rollback firmware.

## Build on the Pi

Extract the source archive into a new folder, open a terminal there, then run:

```sh
scripts/setup.sh --no-build --roms
. deps/env.sh
OS_CMAKE_ARGS="-DSOUND=ON -DAMBIENT=ON -DNIMBUS_TEST=ON -DWEBDIAG=OFF -DC64_PROFILE=OFF" \
  scripts/build-dual-boot.sh build-nimbus-clean
```

Output: `build-nimbus-clean/presto_dual_boot.uf2`.
`NIMBUS_TEST` is the existing option name for enabling Nimbus support; it remains
OFF unless selected. Use a new build directory when changing options. The setup
step fetches the SDK, pinned C64 dependency and game/ROM assets; these are not
included in the source archive. See the existing credits and licensing documents.

## Validation

All three firmware targets built successfully. Every combined UF2 payload was
compared with its source target: 10,973 matching blocks. Controller storage was
checked against the linked save regions and firmware/application partitions.
Native mapping, persistence and reconnect tests passed. The actual embedded HTML
in all three builds was decoded and checked for diagnostics disabled and controller
controls present. C64 static headroom: 41,252 bytes, before runtime allocations.

A new live-browser check of the small diagnostics-visibility change was not run:
automatic approval review could not complete because of a usage-limit error.
Earlier test 5 browser checks and user hardware validation remain applicable to
the unchanged controller behaviour. Existing SDK/newlib build warnings remain.

This work has not been pushed to GitHub or added to the pending C64 PR.
