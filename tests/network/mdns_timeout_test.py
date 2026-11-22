"""Run the pinned lwIP responder in memory; no sockets or Presto are accessed.

The old timer budget must fail on overlapping queries, the production budget
must pass. Host-only changes are pointer alignment, a deterministic clock/random
source, assertion reporting and LWIP_TESTMODE's timeout-list inspection.
"""
from pathlib import Path
import os
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
LWIP = ROOT / "deps/pico-sdk/lib/lwip/src"
ASSERTION = "sys_timeout: timeout != NULL, pool MEMP_SYS_TIMEOUT is empty"

with tempfile.TemporaryDirectory(prefix="presto-mdns-") as directory:
    out = Path(directory)
    (out / "arch").mkdir()
    (out / "arch/cc.h").write_text(r'''
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#ifndef BYTE_ORDER
#define BYTE_ORDER LITTLE_ENDIAN
#endif
#define LWIP_DONT_PROVIDE_BYTEORDER_FUNCTIONS
typedef unsigned long sys_prot_t;
#define LWIP_RAND() ((uint32_t)0)
void host_assert(const char *, const char *, int);
#define LWIP_PLATFORM_ASSERT(x) host_assert(x, __FILE__, __LINE__)
#define LWIP_PLATFORM_DIAG(x) do { printf x; } while (0)
''')
    (out / "arch/sys_arch.h").write_text("typedef unsigned long sys_prot_t;\n")
    (out / "lwipopts.h").write_text(
        '#include "' + str(ROOT / "src/boards/presto/lwipopts.h") + '"\n' + r'''
/* Host pointers are 64-bit; firmware's 32-bit alignment remains unchanged. */
#undef MEM_ALIGNMENT
#define MEM_ALIGNMENT 8
#ifdef TEST_OLD_TIMER_BUDGET
#undef MEMP_NUM_SYS_TIMEOUT
#define MEMP_NUM_SYS_TIMEOUT (LWIP_NUM_SYS_TIMEOUT_INTERNAL + 4)
#endif
''')
    sources = sorted((LWIP / "core").glob("*.c"))
    sources += sorted((LWIP / "core/ipv4").glob("*.c"))
    sources += [LWIP / "netif/ethernet.c", LWIP / "apps/mdns/mdns_out.c",
                LWIP / "apps/mdns/mdns_domain.c",
                ROOT / "tests/network/mdns_timeout_test.c"]
    common = [os.environ.get("CC", "cc"), "-O1", "-g",
              "-fsanitize=address,undefined", "-DLWIP_TESTMODE=1",
              "-I" + str(out), "-I" + str(LWIP / "include"),
              "-I" + str(LWIP / "apps/mdns")]
    env = dict(os.environ, ASAN_OPTIONS="halt_on_error=1",
               UBSAN_OPTIONS="halt_on_error=1")
    for old in (True, False):
        binary = out / ("old-budget" if old else "production")
        flags = ["-DTEST_OLD_TIMER_BUDGET=1"] if old else []
        subprocess.run(common + flags + list(map(str, sources)) + ["-o", str(binary)], check=True)
        for mode in (("spaced", "burst") if old else ("spaced", "burst", "lifecycle")):
            result = subprocess.run([str(binary), mode], capture_output=True, text=True, env=env)
            if old and mode == "burst":
                assert result.returncode == 86 and ASSERTION in result.stderr, result.stdout + result.stderr
                print("Old budget: reproduced the timeout-pool assertion")
            else:
                assert result.returncode == 0 and "PASS" in result.stdout, result.stdout + result.stderr
                print(("Old budget: " if old else "Production budget: ") + result.stdout.strip())
