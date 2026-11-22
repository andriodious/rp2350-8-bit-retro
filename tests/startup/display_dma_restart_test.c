/* Host DMA model for the production frame ISR and recovery functions. */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef unsigned uint;
#define __not_in_flash_func(name) name
#define count_of(a) (sizeof(a) / sizeof((a)[0]))
#define BOARD_DISPLAY_WIDTH 480
#define BOARD_DISPLAY_HEIGHT 480
#if MACHINE_C64 || MACHINE_SPECTRUM
#define BOARD_DISPLAY_ROW_WORDS 241
#else
#define BOARD_DISPLAY_ROW_WORDS 61
#endif
#define TIMING_V_BACK 13
#define TIMING_V_DISPLAY 493
#define TIMING_V_FRONT 589
#define DMA_CH0_CTRL_TRIG_EN_BITS 1u
#define st7701_rgbe_offset_row_start 0
#define st7701_timing_wrap_target 0
enum pio_src_dest { pio_x, pio_y, pio_isr, pio_osr, pio_null };

struct channel {
    union { uint32_t ctrl_trig, al1_ctrl; };
    uintptr_t read_addr;
    uint32_t count;
    bool busy;
};
static struct { struct channel ch[8]; } hardware;
static __typeof__(hardware) *dma_hw = &hardware;
struct pio { uint32_t irq; bool enabled, empty; unsigned clears, restarts, execs, puts; };
typedef struct pio *PIO;
static struct pio pixels, timing;
static PIO lcd_pio = &pixels, timing_pio = &timing;
static uint parallel_sm, timing_sm, timing_offset = 3;
static uint lcd_dma = 0, lcd_dma2 = 1;
static uint timing_dma = 2, timing_dma2 = 3, frame_dma = 4, frame_dma2 = 5;
static uint32_t timing_table[TIMING_V_FRONT * 4];
static const uint32_t *timing_table_addr = timing_table;
static const uint16_t *const *volatile row_table, *const *volatile row_table_1;
static const uint16_t *const *pending_rows;
static uint32_t frame_count, frame_time_us, display_offset_us, line_ns;
static uint32_t late_max_us, late_frames, missed_frames, resyncs, last_isr_us, period_us;
static uint16_t old_pixels[BOARD_DISPLAY_WIDTH + 2], new_pixels[BOARD_DISPLAY_WIDTH + 2];
static const uint16_t *old_rows[481], *new_rows[481];
static const uint16_t *const *expected_rows;
static unsigned aborts, starts, enables, disables, wakeups, now;
static unsigned inject_step, inject_source, aborted_mask;
static bool recovering;

static unsigned channel_count(void) { return PRESTO_LCD_TIMING_DMA ? 6 : 2; }
static uint32_t time_us_32(void) { return now; }
static void __sev(void) { wakeups++; }
static bool pio_sm_is_tx_fifo_empty(PIO p, uint sm) { (void)sm; return p->empty; }
static bool dma_channel_is_busy(uint c) { return hardware.ch[c].busy; }
static void trigger(uint c) {
    if (hardware.ch[c].al1_ctrl & DMA_CH0_CTRL_TRIG_EN_BITS) {
        assert(!recovering); /* No DMA may restart between first abort and rearm. */
        hardware.ch[c].busy = true;
    }
}
/* Model completions and writes to trigger aliases, including writes already in
 * flight when EN was cleared. A disabled destination must not start. */
static void complete(uint c) {
    hardware.ch[c].busy = false;
    if (c == lcd_dma) trigger(lcd_dma2);
    if (c == lcd_dma2 || c == frame_dma2) {
        hardware.ch[lcd_dma].read_addr = (uintptr_t)old_pixels;
        trigger(lcd_dma);
    }
    if (c == timing_dma) trigger(timing_dma2);
    if (c == timing_dma2) {
        hardware.ch[timing_dma].read_addr = (uintptr_t)timing_table;
        trigger(timing_dma);
        trigger(frame_dma);
    }
    if (c == frame_dma) {
        hardware.ch[lcd_dma2].read_addr = (uintptr_t)&old_rows[1];
        trigger(frame_dma2);
    }
}
static void hw_clear_bits(uint32_t *reg, uint32_t bits) {
    *reg &= ~bits;
    if (reg == &pixels.irq) return;
    assert(bits == DMA_CH0_CTRL_TRIG_EN_BITS);
    disables++;
    if (disables == inject_step) complete(inject_source);
}
static void dma_channel_abort(uint c) {
    for (unsigned i = 0; i < channel_count(); i++) assert(!(hardware.ch[i].al1_ctrl & 1));
    assert(!pixels.enabled);
    if (PRESTO_LCD_TIMING_DMA) assert(!timing.enabled);
    recovering = true;
    /* Every source can have its last write/chain still pending at abort. */
    complete(c);
    hardware.ch[c].count = 0;
    aborted_mask |= 1u << c;
    aborts++;
}
static void assert_rebuilt(void) {
    assert(aborts == channel_count());
    assert(aborted_mask == (1u << channel_count()) - 1);
    assert(row_table == expected_rows && !pending_rows);
    assert(hardware.ch[lcd_dma].read_addr == (uintptr_t)expected_rows[0]);
    assert(hardware.ch[lcd_dma2].read_addr == (uintptr_t)&expected_rows[1]);
    assert(hardware.ch[lcd_dma].count == BOARD_DISPLAY_ROW_WORDS);
    assert(hardware.ch[lcd_dma2].count == 1);
    assert(pixels.clears == 1 && pixels.restarts == 1 && pixels.execs);
    assert(!(pixels.irq & ((1u << 0) | (1u << 4))));
    if (PRESTO_LCD_TIMING_DMA) {
        assert(hardware.ch[timing_dma].read_addr == (uintptr_t)timing_table);
        assert(hardware.ch[timing_dma].count == count_of(timing_table));
        assert(hardware.ch[timing_dma2].read_addr == (uintptr_t)&timing_table_addr);
        assert(hardware.ch[timing_dma2].count == 1);
        assert(hardware.ch[frame_dma].read_addr == (uintptr_t)&row_table_1);
        assert(hardware.ch[frame_dma2].read_addr == (uintptr_t)&expected_rows[0]);
        assert(hardware.ch[frame_dma].count == 1 && hardware.ch[frame_dma2].count == 1);
        assert(row_table_1 == &expected_rows[1]);
        assert(timing.clears == 1 && timing.restarts == 1 && timing.execs == 1);
        assert(!(pixels.irq & (1u << 1)));
    } else assert(!timing.clears && !timing.restarts && !timing.execs && timing.enabled);
}
static void hw_set_bits(uint32_t *reg, uint32_t bits) {
    assert(bits == DMA_CH0_CTRL_TRIG_EN_BITS);
    assert_rebuilt();
    *reg |= bits;
    enables++;
}
static void dma_channel_set_read_addr(uint c, const volatile void *p, bool start) {
    assert(!start && aborts == channel_count() && !enables);
    hardware.ch[c].read_addr = (uintptr_t)p;
}
static void dma_channel_set_trans_count(uint c, uint32_t n, bool start) {
    assert(!start && aborts == channel_count() && !enables);
    hardware.ch[c].count = n;
}
static void dma_channel_start(uint c) {
    assert(enables == channel_count());
    assert_rebuilt();
    assert(pixels.enabled);
    assert(c == (starts ? timing_dma : lcd_dma));
    assert(!PRESTO_LCD_TIMING_DMA || !timing.enabled);
    recovering = false;
    trigger(c);
    starts++;
}
static void pio_sm_set_enabled(PIO p, uint sm, bool on) {
    (void)sm;
    if (on) {
        assert(enables == channel_count());
        if (p == timing_pio) assert(starts == 2);
    }
    p->enabled = on;
}
static void pio_sm_clear_fifos(PIO p, uint sm) {
    (void)sm; assert(!p->enabled && aborts == channel_count()); p->empty = true; p->clears++;
}
static void pio_sm_restart(PIO p, uint sm) { (void)sm; assert(!p->enabled); p->restarts++; }
static uint pio_encode_jmp(uint pc) { return pc; }
static uint pio_encode_pull(bool a, bool b) { (void)a; (void)b; return 0x8000; }
static uint pio_encode_mov(uint a, uint b) { (void)a; (void)b; return 0xa000; }
static uint pio_encode_out(uint a, uint b) { (void)a; (void)b; return 0x6000; }
static void pio_sm_exec(PIO p, uint sm, uint instr) {
    (void)sm; assert(!p->enabled);
    if (p == timing_pio) assert(instr == timing_offset + st7701_timing_wrap_target);
    p->execs++;
}
static void sm_exec(uint instr) { pio_sm_exec(lcd_pio, parallel_sm, instr); }
static uint32_t pins_for(uint16_t rgb) { return rgb; } /* Ordering test, not pixel encoding. */
static void pio_sm_put(PIO p, uint sm, uint32_t word) { (void)sm; (void)word; assert(!p->enabled); p->puts++; }
static void pio_interrupt_clear(PIO p, uint irq) { assert(!p->enabled); p->irq &= ~(1u << irq); }

#include "display_dma_under_test.h"

static void setup(unsigned busy_mask, bool change_rows) {
    memset(&hardware, 0, sizeof hardware); memset(&pixels, 0, sizeof pixels); memset(&timing, 0, sizeof timing);
    for (unsigned i = 0; i < 480; i++) { old_rows[i] = old_pixels; new_rows[i] = new_pixels; }
    old_rows[480] = new_rows[480] = NULL;
    row_table = old_rows; row_table_1 = &old_rows[1]; pending_rows = NULL;
    expected_rows = change_rows ? new_rows : old_rows;
    if (change_rows) board_display_set_rows(new_rows);
    assert(board_display_get_rows() == expected_rows);
    for (unsigned i = 0; i < 8; i++) { hardware.ch[i].al1_ctrl = 0x201; hardware.ch[i].busy = !!(busy_mask & (1u << i)); }
    hardware.ch[timing_dma].read_addr = (uintptr_t)&timing_table[(TIMING_V_DISPLAY + 10) * 4];
    hardware.ch[lcd_dma2].read_addr = (uintptr_t)&old_rows[5];
    pixels.irq = (1u << 0) | (1u << 1) | (1u << 4) | (1u << 6);
    pixels.enabled = timing.enabled = true;
    aborts = starts = enables = disables = wakeups = aborted_mask = 0;
    recovering = false; now = 100000; frame_count = last_isr_us = resyncs = late_frames = missed_frames = 0;
    period_us = 20000; line_ns = 34000; display_offset_us = 109 * 34;
}
static void check_recovery(void) {
    frame_start_isr();
    assert_rebuilt();
    assert(frame_count == 1 && wakeups == 1);
    assert(starts == (PRESTO_LCD_TIMING_DMA ? 2 : 1));
    assert(resyncs == (PRESTO_LCD_TIMING_DMA ? 1 : 0));
    assert(pixels.enabled && timing.enabled && (pixels.irq & (1u << 6)));
    assert(frame_time_us == now + (PRESTO_LCD_TIMING_DMA ? TIMING_V_BACK * 34 : display_offset_us));
    for (unsigned i = 0; i < 8; i++) assert(hardware.ch[i].al1_ctrl == 0x201); /* Other DMA/config bits untouched. */
#if MACHINE_C64 || MACHINE_SPECTRUM
    assert(pixels.puts == 1);
#else
    assert(pixels.puts == 3);
#endif
}
int main(void) {
    /* All initial busy combinations, both pending/no-pending mode changes. */
    for (unsigned mask = 0; mask < (1u << channel_count()); mask++) {
        for (unsigned change = 0; change < 2; change++) {
            setup(mask, change); inject_step = 0; check_recovery();
        }
    }
    /* A source completes at each boundary while the graph is being disabled. */
    for (unsigned step = 1; step <= channel_count(); step++) {
        for (unsigned source = 0; source < channel_count(); source++) {
            setup((1u << channel_count()) - 1, true);
            inject_step = step; inject_source = source; check_recovery();
        }
    }
#if PRESTO_LCD_TIMING_DMA
    setup(0, true); pixels.empty = true;
    hardware.ch[lcd_dma2].read_addr = (uintptr_t)&old_rows[481];
    frame_start_isr(); /* Healthy scanout publishes a mode change without disturbing DMA/PIO. */
    assert(!aborts && !disables && !starts && !resyncs && row_table == new_rows && !pending_rows);
    assert(row_table_1 == &new_rows[1] && hardware.ch[frame_dma2].read_addr == (uintptr_t)&new_rows[0]);
    setup(63, true);
    hardware.ch[timing_dma].read_addr = (uintptr_t)&timing_table[(TIMING_V_FRONT - 1) * 4];
    frame_start_isr(); /* Too late to publish: leave the active frame and pending table intact. */
    assert(late_frames == 1 && !aborts && !disables && !starts && row_table == old_rows && pending_rows == new_rows);
#endif
    setup(0, true); pixels.irq = 0; frame_start_isr();
    assert(!frame_count && !aborts && !disables && !starts && pending_rows == new_rows);
    puts("PASS: production display ISR, abort races, row publication, timing phase and rearm");
}
