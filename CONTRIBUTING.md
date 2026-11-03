# Contributing

Use this project's issue tracker and pull requests once its repository is
published. BBC-specific fixes may also be suitable for the original upstream;
keep their scope clear and follow that maintainer's preferences.

Keep machine-specific code in `src/machines`, shared UI/input/storage code in
`src/common`, and Presto hardware code in `src/boards/presto`. Preserve emulator
source notices and dependency pins. Do not commit downloaded ROMs, games, build
outputs or Wi-Fi secrets. Follow `AGENTS.md` for timing/render changes.

Build all three targets after shared changes. For controller work, run the native
mapping, reconnect and persistence tests in `tests/bluetooth`, and the browser
checks when the interface changes. Keep firmware payload/layout verification.
Report hardware results separately from compilation or host tests. Display/JIT
changes must retain the existing frame-exact validation requirements.
