#pragma once
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define __force_inline inline __attribute__((always_inline))
#define __no_inline_not_in_flash_func(name) name
typedef struct { volatile uint32_t timing, rfmt, rcmd, wfmt, wcmd; } mock_qmi_window;
static struct { mock_qmi_window m[2]; } mock_qmi;
static struct { volatile uint32_t ctrl, ctr_hit, ctr_acc; } mock_xip;
static struct { volatile uint32_t timerawh, timerawl; } mock_timer;
static struct { struct { volatile uint32_t ctrl, div; } clk[10]; } mock_clocks;
static struct { volatile uint32_t prim, fbdiv_int; } mock_pll;
static uint32_t mock_bootram[16];
static uint16_t mock_sram[16];
#define qmi_hw (&mock_qmi)
#define xip_ctrl_hw (&mock_xip)
#define timer0_hw (&mock_timer)
#define clocks_hw (&mock_clocks)
#define pll_sys_hw (&mock_pll)
#define SRAM_BASE ((uintptr_t)mock_sram)
#define SRAM_END (SRAM_BASE + sizeof mock_sram)
#define BOOTRAM_BASE ((uintptr_t)mock_bootram)
#define BOOTRAM_END (BOOTRAM_BASE + sizeof mock_bootram)
enum { clk_sys=5, ROM_DATA_FLASH_DEVINFO16_PTR=17 };
static const uint16_t *mock_devinfo_literal;
static bool mock_missing_literal, in_capture;
static unsigned lookup_calls, clock_calls, lcd_calls, flash_calls;
static void *rom_data_lookup(unsigned key) {
    assert(!in_capture && key==ROM_DATA_FLASH_DEVINFO16_PTR);
    lookup_calls++;
    return mock_missing_literal ? NULL : &mock_devinfo_literal;
}
static uint32_t clock_get_hz(unsigned clock) {
    assert(!in_capture && clock==clk_sys);
    clock_calls++;
    return 250000000;
}
static void board_display_counters(uint32_t *a,uint32_t *b,uint32_t *c,uint32_t *d) {
    assert(!in_capture);
    lcd_calls++;
    *a=101;*b=202;*c=303;*d=404;
}
static uint32_t mem_flash_writes(void) {
    assert(!in_capture);
    flash_calls++;
    return 23;
}
