// Diagnostic only: observe ROM return without fetching LED code from flash.
#include "pico/stdlib.h"
#include "pico/bootrom.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "hardware/structs/timer.h"
#include "ws2812.pio.h"
#include "startup_diag.h"

// Kept in SRAM for debugger inspection; no persistent or flash writes.
volatile uint32_t partition_diag_flash_word;
volatile int partition_diag_result;

static void __no_inline_not_in_flash_func(partition_led)(PIO pio, uint sm,
                                                        uint count, uint colour) {
    for (uint i = 0; i < 7; ++i) {
        while (pio->fstat & (1u << (PIO_FSTAT_TXFULL_LSB + sm))) {}
        pio->txf[sm] = i < count ? colour << 8 : 0;
    }
    uint32_t start = timer0_hw->timerawl;
    while ((uint32_t)(timer0_hw->timerawl - start) < 600u) {}
}

static void __no_inline_not_in_flash_func(partition_stop)(PIO pio, uint sm, uint count) {
    partition_led(pio, sm, count, 0x000808u); // magenta: diagnostic failure
    for (;;) __asm volatile ("nop");
}

static int __no_inline_not_in_flash_func(partition_call)(PIO pio, uint sm,
                                                        uint8_t *work, uint32_t size) {
    partition_led(pio, sm, 1, 0x080008u); // cyan, before uncached physical flash read
    partition_diag_flash_word = *(volatile uint32_t *)XIP_NOCACHE_NOALLOC_NOTRANSLATE_BASE;
    partition_led(pio, sm, 2, 0x080008u);
    rom_load_partition_table_fn fn = (rom_load_partition_table_fn)
        rom_func_lookup_inline(ROM_FUNC_LOAD_PARTITION_TABLE);
    if (!fn) partition_stop(pio, sm, 1);
    partition_led(pio, sm, 3, 0x080008u);
    if (!bootrom_try_acquire_lock(BOOTROM_LOCK_SHA_256)) partition_stop(pio, sm, 2);
    partition_led(pio, sm, 4, 0x080008u);
    int rc = fn(work, size, false);
    partition_diag_result = rc;
    partition_led(pio, sm, 5, 0x080008u);
    bootrom_release_lock(BOOTROM_LOCK_SHA_256);
    partition_led(pio, sm, 6, 0x080008u);
#if !LAUNCHER_BOOT_REPORT
    if (rc < 0) partition_stop(pio, sm, 3);
#endif
    return rc;
}

int partition_diag_load(uint8_t *work, uint32_t size) {
    PIO pio = pio0;
    if (pio_set_gpio_base(pio, 16) != PICO_OK) goto unavailable;
    int sm = pio_claim_unused_sm(pio, false);
    if (sm < 0) goto unavailable;
    if (!pio_can_add_program(pio, &ws2812_program)) {
        pio_sm_unclaim(pio, sm);
        goto unavailable;
    }
    uint offset = pio_add_program(pio, &ws2812_program);
    pio_gpio_init(pio, 33);
    pio_sm_set_consecutive_pindirs(pio, sm, 33, 1, true);
    pio_sm_config c = ws2812_program_get_default_config(offset);
    sm_config_set_sideset_pins(&c, 33);
    sm_config_set_out_shift(&c, false, true, 24);
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);
    sm_config_set_clkdiv(&c, (float)clock_get_hz(clk_sys) / 8000000.0f);
    pio_sm_init(pio, sm, offset, &c);
    pio_sm_set_enabled(pio, sm, true);
    int rc = partition_call(pio, sm, work, size);
    // Reaching this marker also tests the return to flash-backed launcher code.
    partition_led(pio, sm, 7, 0x080008u);
    pio_sm_set_enabled(pio, sm, false);
    pio_remove_program(pio, &ws2812_program, offset);
    pio_sm_unclaim(pio, sm);
    return rc;
unavailable:
    startup_diag_mark(0x101);
    // Do not silently bypass the diagnostic when its PIO resource is unavailable.
    for (;;) tight_loop_contents();
}
