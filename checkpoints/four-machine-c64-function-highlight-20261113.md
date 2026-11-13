# C64 function-key highlighting checkpoint — 13 November 2026

Tag: `reference/four-machine-c64-function-highlight-20261113`.
Previous checkpoint `reference/four-machine-keyboard-mappings-20261112` is preserved. Stable v0.5.0 and published assets are unchanged. Local checkpoint only; no push or release requested.

## Exact tested firmware

Parent workspace artifact: `artifacts/four-machine-c64-function-highlight-20261113/presto-four-machine-c64-function-highlight.uf2`.
SHA256: `2659526df29bffefc9be2476503aa482c24b59ada09a045c3557f5ff52f6b7cd`.
Images: Master `6e9c4e5abe1be093`, BBC `0b2a8a66a6947296`, C64 `c38358cccace6656`, Spectrum `0576454036335598`.
Launcher SHA256: `f33e511f43f3ff0f9c57ec48d99eea907919595f3c06b58c5eefa02adea0f32b`.
Preserve this exact binary; rebuilding after checkpointing may change its identity.

## Change and verification

C64 web function buttons show the pressed state and clear it on release, pointer leave or cancellation. Key commands and joystick handling are unchanged. The reported web Fire issue was resolved by enabling joystick mode in Elite.

User confirmed all four function-key buttons highlight and still trigger their Elite actions on C64 image `c38358cccace6656`. Status showed the Classic keyboard connected, zero invalid reports, no recorded C64 fault and 24,168 bytes free RAM. Nimbus was not connected in this keyboard-only test.

Software validation: all four function buttons passed browser highlight, unchanged key-command, release and cancellation checks; combined firmware payload and partition checks passed; C64 static headroom remains 30,248 bytes.

Existing D81 optimisations, keyboard mappings, BOOT status/SD chooser and Wi-Fi/Nimbus functionality are retained. This confirmation covers the C64 highlight fix, not completion of every outstanding four-machine mapping test from the previous checkpoint. Settings v2 compatibility remains unchanged.
