# Contributing

Use this project's GitHub issue tracker and pull requests. BBC-specific fixes may also be suitable for the original upstream;
keep their scope clear and follow that maintainer's preferences.

Keep machine-specific code in `src/machines`, shared UI/input/storage code in
`src/common`, and Presto hardware code in `src/boards/presto`. Preserve emulator
source notices and dependency pins. Do not commit downloaded ROMs, games, build
outputs or Wi-Fi secrets. Follow `AGENTS.md` for timing/render changes.

Build all four machine targets (Master, BBC Model B, C64 and Spectrum) plus the launcher after shared changes. For controller work, run the native
mapping, reconnect and persistence tests in `tests/bluetooth`, and the browser
checks when the interface changes. Keep firmware payload/layout verification.
Report hardware results separately from compilation or host tests. Display/JIT
changes must retain the existing frame-exact validation requirements.

For the current release configuration, use `scripts/build-classic-keyboard-four.sh build-release` after dependency setup. Test Classic keyboard and Nimbus independently and together, capture/add/delete mappings, reconnect and per-machine persistence. See [build instructions](docs/build-retro.md).
