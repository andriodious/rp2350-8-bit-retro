# Host checks

From the repository root, using a host C/C++ compiler with sanitizers:

```
cc -fsanitize=address,undefined tests/c64/disk_bounds.c -o /tmp/c64-disk-test
/tmp/c64-disk-test
c++ -std=c++20 -DFRODO_RP2350 -fsanitize=address,undefined tests/c64/prefs_layout.cpp deps/frank-c64/src/rp2350/Prefs_rp2350.cpp -o /tmp/c64-prefs-test
/tmp/c64-prefs-test
python3 tests/c64/packer_test.py
```

The browser check requires Playwright. It intercepts all requests with a simulated API; it does not connect to a Presto. Supply `CHROME_PATH` for an existing Chrome executable, or install Playwright's Chromium. `SCREENSHOT` optionally chooses the screenshot output path.

```
node tests/c64/web_test.cjs
```

The checks cover virtual-disk bounds and read-only access, construction/copy of the real preferences class, UF2 partition relocation and overflow/overlap rejection, and browser keyboard/file-type controls. They do not execute the ARM firmware or validate emulation accuracy, audio, LCD timing or device stability.

Revision 2 also includes `http_lifetime_test.py` (generates /tmp/http-lifetime.c for sanitizer compilation), and `native/build.sh` (run with bash; compiles and runs the emulator with the embedded Elite disk under host sanitizers). The native harness does not simulate Presto hardware.


Before these checks, fetch the pinned dependencies with `python3 scripts/fetch-c64.py`.
Additional current checks:

```sh
python3 tests/c64/app_metadata_test.py
python3 tests/c64/audio_test.py
python3 tests/c64/http_lifetime_test.py
cc -fsanitize=address,undefined /tmp/http-lifetime.c -o /tmp/http-lifetime
/tmp/http-lifetime
python3 tests/c64/launcher_fallback_test.py
cc -fsanitize=address,undefined /tmp/launcher-r5-test.c -o /tmp/launcher-fallback
/tmp/launcher-fallback
bash tests/c64/native/build.sh
node tests/c64/webdiag_test.cjs
python3 tests/c64/dependency_test.py /path/frank-c64-main.zip /path/elite-commodore-64-gma86-pal.d64
```

Native harness leak detection is disabled; its frame checks do not establish absence of memory leaks. See `docs/c64-validation.md` for this fileset's results and hardware limitations.

## T64 program loading

See [T64 validation](../../docs/c64-t64.md#validation-and-hardware-test) for the
parser sanitizer, production API handler, native C64 execution and browser tests.
The native harness accepts `sh tests/c64/native/build.sh t64` to test generated
T64 programs instead of the existing Elite disk run.

## CRT and P00

`sh tests/c64/native/build.sh media` runs bounded cartridge loading, allocation
failure, bank/reset mapping and P00 execution tests. See
[cartridge/P00 notes](../../docs/c64-cartridge-p00.md) for optional sample tests and
hardware validation. No copyrighted sample games are included.
