#ifndef C64_RUNTIME_DIAG_H
#define C64_RUNTIME_DIAG_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum c64_runtime_diag_event {
    C64_RUNTIME_DIAG_BOOT_READY = 1,
    C64_RUNTIME_DIAG_MENU_OPEN_BEGIN,
    C64_RUNTIME_DIAG_MENU_OPEN_END,
    C64_RUNTIME_DIAG_MENU_CLOSE_BEGIN,
    C64_RUNTIME_DIAG_MENU_CLOSE_END,
    C64_RUNTIME_DIAG_FLASH_BEGIN,
    C64_RUNTIME_DIAG_FLASH_END
} c64_runtime_diag_event_t;

#define C64_RUNTIME_DIAG_EVENT_CAPACITY 8u
#define C64_RUNTIME_DIAG_JSON_CAPACITY 8192u

#if defined(C64_RUNTIME_DIAG) && C64_RUNTIME_DIAG && defined(MACHINE_C64) && MACHINE_C64
/* Call once on core 0 while flash is available. Resolves the ROM devinfo pointer;
 * recording starts afterwards. The first BOOT_READY capture is kept separately
 * as an immutable baseline. init() is idempotent and does not record an event. */
void c64_runtime_diag_init(void);

/* Core 0 only: SRAM code, SRAM storage and read-only MMIO. Safe inside the flash
 * lockout callback, including with interrupts disabled. No allocation, logging,
 * SDK/ROM lookup or read-to-clear register accesses. Flash events use their
 * sector offset as arg; menu/boot events use zero. Calls before init are ignored.
 * The last eight events are retained; sequence wraps modulo 2^32. */
void c64_runtime_diag_capture(c64_runtime_diag_event_t event, uint32_t arg);

/* Core 0, with flash available: serialize a standalone JSON object (at most
 * 8191 bytes plus NUL), including schema labels, baseline, chronological events,
 * current read-only register values, LCD counters and flash-write count. A
 * short buffer receives a complete error object, or {} if only three bytes fit;
 * size < 3 cannot hold a JSON object and receives an empty string if possible.
 * Register arrays contain unsigned decimal words. Timer timestamps are raw
 * timer-0 microseconds. configuredSysHz is SDK configuration, not a measurement.
 * devinfo is null if the ROM pointer was absent or outside SRAM/BOOTRAM. */
void c64_runtime_diag_json(char *out, size_t size);

/* Appendable comma-prefixed status fragment. changed mask: bit 0 QMI M0,
 * bit 1 QMI M1, bit 2 XIP CTRL, bit 3 BOOTRAM[0..4], bit 4 clocks/PLL,
 * bit 5 devinfo. The mask is null until a boot baseline exists. LCD array:
 * [late_max_us, resyncs, missed_frames, late_frames], cumulative since boot.
 * On insufficient space returns a complete error fragment, or an empty string.
 * This function, like json(), must never run inside a flash lockout. */
void c64_runtime_diag_summary_json(char *out, size_t size);
#else
static inline void c64_runtime_diag_init(void) {}
static inline void c64_runtime_diag_capture(c64_runtime_diag_event_t event, uint32_t arg) {
    (void)event; (void)arg;
}
static inline void c64_runtime_diag_json(char *out, size_t size) {
    if (out && size >= 3) { out[0] = '{'; out[1] = '}'; out[2] = 0; }
    else if (out && size) out[0] = 0;
}
static inline void c64_runtime_diag_summary_json(char *out, size_t size) {
    if (out && size) out[0] = 0;
}
#endif

#ifdef __cplusplus
}
#endif
#endif
