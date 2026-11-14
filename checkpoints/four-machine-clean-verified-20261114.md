# Verified clean four-machine build — 14 November 2026

Reference tag: `reference/four-machine-clean-verified-20261114`, pointing to exact tested source commit `554a331edc77e2799dd339d43717a86d820eb481`.
The verification record is a subsequent documentation-only commit on `experiment/c64-1581`.

## Preserved tested artifact

Parent workspace path: `artifacts/clean-github-verification-20261113/presto-four-machine-clean-verification.uf2`.
SHA256: `b4c5c61a75b5c2b5b7f4e46106abf42bdf0907ef9d1ab916f5e8b62c9fa047e0`.

| Machine | Image ID |
| --- | --- |
| Master | `2a64aa1e8472b3e6` |
| BBC Micro | `d7ba673b71f8c70b` |
| C64 | `0e3cf7917e31fb59` |
| Spectrum | `7fcfaed1dc5fc173` |

## Hardware verification

User reports all four machines work correctly after flashing and power cycling. Classic keyboard and Nimbus reconnect; saved mappings are retained; C64 web function keys highlight correctly; sound and web responsiveness are satisfactory. This records the user's hardware results, not an automated hardware test.

## Clean-build verification

Fresh GitHub checkout of `experiment/c64-1581` at the source commit above, with no source modifications or reused firmware build directory. All four machines and launcher compiled successfully. ROMs and Elite assets downloaded; Spectrum fork matched all 36 vendored chips/systems files at pinned revision `4e29e48867477a39c93173750a3902bc470f34d0`. Combined UF2 exactly matched 14,739 payload blocks, partition checks passed, and C64 static headroom was 30,248 bytes.

Build entry point: `sh scripts/build-classic-keyboard-four.sh`, using Arm GNU 14.2.rel1, Pico SDK 2.2.0 and Rust 1.99.1 with thumbv8m.main-none-eabi. Setup includes the separate Master Elite download specified in scripts/setup.sh. Preserve the exact UF2 above; source identity and build metadata can change rebuilt image IDs.

Previous references `reference/four-machine-c64-function-highlight-20261113` and `reference/four-machine-keyboard-mappings-20261112` and their exact artifacts remain preserved. Stable v0.5.0, main, and published release assets are unchanged. No new stable release is made.
