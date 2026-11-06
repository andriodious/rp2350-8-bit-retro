// Diagnostic pre-main probe: GPIO/float coprocessors must be enabled first.
#include "pico/runtime.h"
#include "pico/platform.h"
#include "startup_diag.h"
#ifndef STARTUP_DIAG_LAUNCHER
#define STARTUP_DIAG_LAUNCHER 0
#endif
static void startup_runtime_ready(void) {
    // Per-core initializers also run when the emulator launches core 1.
    if (get_core_num() != 0) return;
    startup_diag_mark(STARTUP_DIAG_LAUNCHER ? 0x402 : 0x405);
}
PICO_RUNTIME_INIT_FUNC_PER_CORE(startup_runtime_ready, "01201");
int __real_main(void);
int __wrap_main(void) {
    startup_diag_mark(STARTUP_DIAG_LAUNCHER ? 0x404 : 0x407);
    return __real_main();
}
