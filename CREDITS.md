# Credits and provenance

This platform builds on the following work. Original copyright, licence files,
source headers and Git history are retained; integration work does not claim
ownership of the emulator cores, ROMs or games.

- **8bitkick and contributors — rp2350-bbc-micro:** the original Presto BBC
  platform, RP2350 integration/JIT work, display, web/SD interfaces and supporting
  tools. https://github.com/8bitkick/rp2350-bbc-micro
- **Chris Evans and contributors — beebjit:** BBC Model B emulator.
  https://github.com/scarybeasts/beebjit
- **Kieran Connell and contributors:** the Master emulator integration's kc-beeb
  foundation; see source notices and `src/cores/kc-beeb/UPSTREAM`.
- **Mikhail Matveev (rh1tech) and FRANK C64 contributors:** original RP2350 C64
  port. https://github.com/rh1tech/frank-c64
- **Christian Bauer and Frodo contributors:** C64 CPU, video, sound and peripheral
  emulation incorporated by FRANK. See [C64 provenance](docs/c64-credits.md).
- **Ricardo Quesada and Bluepad32 contributors:** original Nimbus protocol/usage
  reference. Bluepad32 source is not compiled into this integration.
- **BlueKitchen:** BTstack Bluetooth host stack. Its licence is retained in
  `docs/licenses/BTstack.txt` and in the SDK dependency.
- **Raspberry Pi, Pimoroni and dependency contributors:** SDK, Presto hardware and
  supporting drivers/libraries, with their original notices retained.
- **Ian Bell, David Braben and the respective Elite copyright owners:** Elite
  game code/assets. Credit does not imply a new licence grant.
- **Mark Moxon:** Elite preservation, documentation and source/build resources.
  https://elite.bbcelite.com and
  https://github.com/markmoxon/elite-source-code-commodore-64
- **The original Acorn/Commodore ROM copyright owners:** ROM assets have separate
  provenance and are not newly authored platform code.

The FRANK dependency is fetched from `andriodious/frank-c64` at the revision pinned
in `third_party/c64-sources.json`. Its original repository/revision, adapted-source
hashes, patch and Elite download checksum remain recorded there. No fork is
silently switched to a moving branch as part of this project transition.

The top-level GPL files are inherited unchanged. Component-specific terms and
asset notices continue to apply; no blanket replacement licence is asserted.
