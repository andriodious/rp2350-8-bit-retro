# Hardware-verified clean flash-read restoration build

On 22 November 2026, the user confirmed: “Build checks out fine”. This followed
requested power-cycle/startup, settings-save, menu-resume and sound checks on
BBC Master, BBC Model B, C64 and Spectrum, plus C64 Monkey Island D81 playback
and physical resets. The confirmation was general; individual counts and new
status samples were not supplied.

The exact clean-build firmware from source commit
`481b5bf6ce9661b9a7a87664fe3a26fd57a579ca` is retained for v0.9.1. It was compiled
from a fresh GitHub checkout before that hardware acceptance, not rebuilt for
publication. Release documentation and CI recipe changes do not alter it.

| Machine | Image ID |
|---|---|
| BBC Master | `1bb01075ee4086ef` |
| BBC Model B | `cb3b0bd1945923ca` |
| C64 | `cab5f1222f5dd3a9` |
| Spectrum | `bde51d736867f106` |

Combined UF2 SHA-256:
`8ccb9e0a0aaf64e57b846d5352a5a847481d8faae7f0f4dad940e451bd99b108`.
Size: 7,731,712 bytes. Launcher SHA-256:
`4f64f88f7b5f22f9d7f0409459142639f6a7045db5d71802d2757f2f14d868fa`.

The [verification record](verification.json) associates the user's acceptance
with the exact firmware. The [earlier candidate measurements](../four-machine-c64-flash-read-restore-20261122/verification.json)
belong to image `634e4a505777a51f`, not this clean-build image.

All four payloads and the launcher compiled from fresh outputs using GNU Arm
14.2.rel1 and Pico SDK 2.2.0. Nine host suites passed, including the reproduced
old quad-read loss and 246 corrected cases. Pinned emulator dependencies and
checksum-protected Elite/1581 assets were verified. Full UF2 payload, SRAM restore
ordering and memory-budget checks passed. C64 runtime diagnostics remain enabled.

The local archive `artifacts/clean-github-flash-read-restore-20261122` contains
build provenance and verification logs. Its original manifest predates hardware
acceptance and remains unchanged; this record adds the subsequent confirmation.
The separate `artifacts/clean-github-flash-read-restore-hardware-verified-20261122`
archive retains a checksummed copy of the accepted firmware.

v0.9.0 and all previous checkpoints remain unchanged. Firmware, downloaded
commercial ROMs, games and Wi-Fi credentials are not added to Git.
