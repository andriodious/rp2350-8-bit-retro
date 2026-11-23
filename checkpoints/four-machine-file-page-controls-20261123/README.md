# Verified file-selection page controls

The user confirmed all four machines passed on 23 November 2026: paging and
file selection with touch, keyboard and Nimbus, including first/last pages,
sound and menu resume. No new individual status samples were supplied.

PG-UP/PG-DOWN move by five titles. Keyboard/Nimbus Left/Right page; Up/Down
retain single-file selection. Touch, BOOT, Return and A/Fire load titles.

The exact tested combined UF2 is retained without rebuilding.
SHA256: `5953b72cce192dfe3a26d1523357d21c9217ff8ddde01970aaf93d65a8200b8d`.

| Machine | Image ID |
|---|---|
| Master | `818b6eff1732e463` |
| BBC Micro | `ff23dc0af19613d4` |
| C64 | `68489f61f0400ff9` |
| Spectrum | `f90a561634f94615` |

See [verification.json](verification.json) for acceptance and build checks.
All four host menu tests passed under address/undefined-behavior sanitizers.
Combined payload verification passed; C64 static headroom remains 26,808 bytes.
Only menu source, documentation and tests changed from v0.9.1.

The local archive `artifacts/four-machine-file-page-controls-20261123` retains
the UF2, matching component ELFs/maps, build settings, logs and source patch.
Its original build-time manifest predates hardware acceptance; the separate
`hardware-verification.json` records the subsequent successful tests.

v0.9.1 and all previous checkpoints remain unchanged. This is a local
checkpoint; no release or GitHub update is made by this checkpoint operation.
