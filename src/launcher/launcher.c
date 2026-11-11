#include "startup_diag.h"
// The launcher (docs/dual-boot.md): what the boot ROM runs first when both machines are in the
// flash. It reads which machine was chosen (the settings sector, system.h) and has the boot ROM
// chain into that machine's slot, mapped to 0x10000000 as if it were the only firmware there.
// No choice, or a choice of nothing there is: the Master. A slot with nothing bootable in it:
// the other one. Neither: the USB bootloader, ready for a .uf2.
//
// The partition table (partitions.json) is in this image's block loop, so the boot ROM has it
// loaded already; it says where each slot is. Nothing else is set up: no clocks beyond the
// SDK's, no USB, no stdio, so the machine starts from much the state it would without us.
#include "pico/stdlib.h"
#include "pico/bootrom.h"
#include "boot/picobin.h"
#include "hardware/flash.h"
#include "hardware/watchdog.h"
#include "hardware/structs/watchdog.h"

#include "board_config.h"
#include "system.h"
#if LAUNCHER_BOOT_REPORT
#include <stdio.h>
#include "pico/stdio_usb.h"
static struct { int slot, lookup, chain; uint32_t first, last; } attempts[4];
static unsigned attempt_count;
static int partition_result;
static boot_info_t boot_info;
static bool boot_info_valid;
static void __attribute__((noreturn)) report_fallback(void) {
    // Only initialise USB after every machine boot attempt has returned.
    // Do not reboot: preserve the return codes and avoid hiding the failure in BOOTSEL.
    watchdog_disable();
    stdio_usb_init();
    for (;;) {
        startup_diag_mark(0x107);
        printf("LAUNCHER_FALLBACK v1 partition=%d boot_valid=%u boot_word=%08lx diagnostic=%08lx\n",
               partition_result, boot_info_valid, (unsigned long)boot_info.boot_word,
               (unsigned long)boot_info.boot_diagnostic);
        for (unsigned i=0; i<attempt_count; ++i)
            printf("ATTEMPT slot=%d lookup=%d first=%lu last=%lu chain=%d\n",
                   attempts[i].slot, attempts[i].lookup, (unsigned long)attempts[i].first,
                   (unsigned long)attempts[i].last, attempts[i].chain);
        stdio_flush();
        sleep_ms(1000);
        startup_diag_mark(0);
        sleep_ms(1000);
    }
}
#endif

// the boot ROM's work area for searching and checking an image (3264 bytes, it says)
static uint8_t s_work[4096] __aligned(4);

// boot the image in a slot: only returns if there's no bootable image there
static void chain(int slot) {
#if LAUNCHER_BOOT_REPORT
    unsigned a = attempt_count++;
    attempts[a].slot = slot;
    attempts[a].chain = 0x7fffffff; // Not called: lookup failed.
#endif
    startup_diag_mark(0x103);
    uint32_t info[3];
    int n = rom_get_partition_table_info(info, 3, PT_INFO_PARTITION_LOCATION_AND_FLAGS | PT_INFO_SINGLE_PARTITION |
                                                  ((uint32_t) slot << 24));
    startup_diag_mark(0x104);
#if LAUNCHER_BOOT_REPORT
    attempts[a].lookup = n;
#endif
    if (n != 3) return;
    uint32_t first = (info[1] & PICOBIN_PARTITION_LOCATION_FIRST_SECTOR_BITS) >> PICOBIN_PARTITION_LOCATION_FIRST_SECTOR_LSB;
    uint32_t last = (info[1] & PICOBIN_PARTITION_LOCATION_LAST_SECTOR_BITS) >> PICOBIN_PARTITION_LOCATION_LAST_SECTOR_LSB;
    startup_diag_mark(0x105);
#if LAUNCHER_BOOT_REPORT
    attempts[a].first = first;
    attempts[a].last = last;
    attempts[a].chain =
#endif
    rom_chain_image(s_work, sizeof s_work, XIP_BASE + first * FLASH_SECTOR_SIZE, (last + 1 - first) * FLASH_SECTOR_SIZE);
    startup_diag_mark(0x106);
}

int main(void) {
    startup_diag_mark(0x101);
#if LAUNCHER_BOOT_REPORT
    boot_info_valid = rom_get_boot_info(&boot_info);
#endif
#if STARTUP_DIAG
    extern int partition_diag_load(uint8_t *, uint32_t);
#if LAUNCHER_BOOT_REPORT
    partition_result =
#endif
    partition_diag_load(s_work, sizeof s_work);
#else
#if LAUNCHER_BOOT_REPORT
    partition_result =
#endif
    rom_load_partition_table(s_work, sizeof s_work, false);
#endif
    startup_diag_mark(0x102);
    const system_settings_t *s = (const system_settings_t *) (XIP_NOCACHE_NOALLOC_BASE + BOARD_FLASH_SYSTEM_OFFSET);
    // Power-on and external reset start Master. Watchdog/software restarts
    // retain the selected machine for web switching and C64 recovery.
    int slot = watchdog_caused_reboot() ? system_slot(system_settings_machine(s)) : SYSTEM_SLOT_MASTER;
    // Only an armed C64 watchdog failure triggers fallback, not a web restart.
    if (watchdog_enable_caused_reboot() && (watchdog_hw->scratch[0] & 0xffff0000u) == C64_DIAG_MAGIC) {
        watchdog_hw->scratch[0] = C64_RETURN_MAGIC | (watchdog_hw->scratch[0] & 511u);
        slot = SYSTEM_SLOT_MASTER;
    }
    chain(slot);
    for (int i = 0; i < 4; i++) {
        const int slots[] = {SYSTEM_SLOT_MASTER, SYSTEM_SLOT_BEEB, SYSTEM_SLOT_C64, SYSTEM_SLOT_SPECTRUM};
        if (slots[i] != slot) chain(slots[i]);
    }
    startup_diag_mark(0x107);
#if LAUNCHER_BOOT_REPORT
    report_fallback();
#else
    reset_usb_boot(0, 0);
#endif
}
