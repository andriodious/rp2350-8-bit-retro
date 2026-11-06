// Linker wrappers keep the SDK/TinyUSB implementation and return values intact.
// Linked only for STARTUP_DIAG; no USB reset/retry policy changes.
#include "pico/stdio_usb.h"
#include "tusb.h"
#include "startup_diag.h"

bool __real_stdio_usb_init(void);
bool __real_tusb_rhport_init(uint8_t rhport, const tusb_rhport_init_t *init);
bool __real_dcd_init(uint8_t rhport, const tusb_rhport_init_t *init);
void __real_rp2040_usb_init(void);

bool __wrap_stdio_usb_init(void) {
    startup_diag_mark(0x301);
    bool result = __real_stdio_usb_init();
    startup_diag_mark(0x401);
    return result;
}
bool __wrap_tusb_rhport_init(uint8_t rhport, const tusb_rhport_init_t *init) {
    startup_diag_mark(0x302);
    bool result = __real_tusb_rhport_init(rhport, init);
    startup_diag_mark(0x307);
    return result;
}
bool __wrap_dcd_init(uint8_t rhport, const tusb_rhport_init_t *init) {
    startup_diag_mark(0x303);
    bool result = __real_dcd_init(rhport, init);
    startup_diag_mark(0x306);
    return result;
}
void __wrap_rp2040_usb_init(void) {
    startup_diag_mark(0x304);
    __real_rp2040_usb_init();
    startup_diag_mark(0x305);
}
