// The flash, by region (mem.h).
#include <string.h>

#include "pico/stdlib.h"
#include "pico/flash.h"
#include "pico/multicore.h"
#include "hardware/flash.h"
#include "hardware/sync.h"
#include "hardware/structs/qmi.h"

#include "mem.h"
#include "c64_runtime_diag.h"
#if BOOT_STATUS_TRIAL
#include "boot_banner.h"
#endif
#include "board_config.h"
#include "system.h"
#include "machine_if.h"

typedef struct {
    uint32_t offset, size;
} region_t;

static const region_t s_layout[MEM_FLASH_REGIONS] = BOARD_FLASH_LAYOUT;

bool mem_flash_region(int region, uint32_t *offset, uint32_t *size) {
    if (region < 0 || region >= MEM_FLASH_REGIONS || !s_layout[region].size) return false;
    *offset = s_layout[region].offset;
    *size = s_layout[region].size;
    // both machines in one flash (system.h): each its own save disc, the Model B's lower down
    if ((region == MEM_FLASH_SAVE_HEAD || region == MEM_FLASH_SAVE_DATA) && !strcmp(machine_name(), "beeb") &&
        system_launched()) *offset -= BOARD_FLASH_SAVE_SLOT;
    return true;
}

const uint8_t *mem_flash_ptr(uint32_t offset) {
    return (const uint8_t *) (XIP_NOCACHE_NOALLOC_NOTRANSLATE_BASE + offset);
}

static struct {
    uint32_t offset, len;
    const uint8_t *data;
} s_w;
static uint8_t s_tail[FLASH_PAGE_SIZE];     // a last part page, padded

static void __no_inline_not_in_flash_func(write_sector)(void *arg) {
    (void) arg;
    // The SDK's saved boot XIP setup can return to serial reads even when this
    // payload started in quad-read mode. Preserve the active read protocol as
    // well as our safe timing. This callback, its saved state and all code up to
    // the restore remain in SRAM, with IRQs off and the other core parked.
    uint32_t timing = qmi_hw->m[0].timing;
    uint32_t rcmd = qmi_hw->m[0].rcmd;
    uint32_t rfmt = qmi_hw->m[0].rfmt;
    // Diagnostic capture also runs from SRAM, inside the existing lockout.
    c64_runtime_diag_capture(C64_RUNTIME_DIAG_FLASH_BEGIN, s_w.offset);
    flash_range_erase(s_w.offset, FLASH_SECTOR_SIZE);
    uint32_t whole = s_w.len & ~(FLASH_PAGE_SIZE - 1);
    if (whole) flash_range_program(s_w.offset, s_w.data, whole);
    if (s_w.len > whole) {
        for (uint32_t i = 0; i < FLASH_PAGE_SIZE; i++) s_tail[i] = whole + i < s_w.len ? s_w.data[whole + i] : 0xff;
        flash_range_program(s_w.offset + whole, s_tail, FLASH_PAGE_SIZE);
    }
    __dsb();
    qmi_hw->m[0].timing = timing;
    qmi_hw->m[0].rcmd = rcmd;
    qmi_hw->m[0].rfmt = rfmt;
    __dsb();
    __isb();
    c64_runtime_diag_capture(C64_RUNTIME_DIAG_FLASH_END, s_w.offset);
}

static uint32_t s_writes;

uint32_t mem_flash_writes(void) {
    return s_writes;
}

bool mem_flash_write_sector(uint32_t offset, const void *data, uint32_t len) {
    if (offset % FLASH_SECTOR_SIZE || len > FLASH_SECTOR_SIZE) return false;
#if BOOT_STATUS_TRIAL
    boot_status_flash_begin();
#endif
    s_writes++;
    s_w.offset = offset;
    s_w.data = data;
    s_w.len = data ? len : 0;
    // (core 1 is parked and core 0's interrupts are off while the flash is busy: the machine
    // pauses for a moment; the panel's rows are fed by DMA alone, so its picture stays)
    if (!multicore_lockout_victim_is_initialized(get_core_num() ^ 1)) {
        // the other core not launched yet (at boot: the app guard turning an app off, the boot
        // guard's "!machine"): it's waiting in the boot ROM, nothing to park, but the SDK refuses
        // (both machines' core 1 sets up its lockout first thing, so this is the only time)
        uint32_t irq = save_and_disable_interrupts();
        write_sector(NULL);
        restore_interrupts(irq);
#if BOOT_STATUS_TRIAL
        boot_status_flash_end();
#endif
        return true;
    }
    bool ok = flash_safe_execute(write_sector, NULL, 1000) == PICO_OK;
#if BOOT_STATUS_TRIAL
    boot_status_flash_end();
#endif
    return ok;
}
