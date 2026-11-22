#include "c64_runtime_diag.h"

#if defined(C64_RUNTIME_DIAG) && C64_RUNTIME_DIAG && defined(MACHINE_C64) && MACHINE_C64
#include "pico/stdlib.h"
#include "pico/bootrom.h"
#include "hardware/clocks.h"
#include "hardware/regs/addressmap.h"
#include "hardware/structs/clocks.h"
#include "hardware/structs/pll.h"
#include "hardware/structs/qmi.h"
#include "hardware/structs/timer.h"
#include "hardware/structs/xip_ctrl.h"
#include "board.h"
#include "mem.h"
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    uint32_t sequence, event, arg, time_lo, time_hi;
    uint32_t qmi[2][5];
    uint32_t xip_ctrl;
    uint32_t bootram[5];
    uint32_t clocks[4];
    uint32_t devinfo;
} runtime_snapshot_t;

/* Volatile word accesses prevent capture's copy from becoming a library call.
 * Both capture and serializers run on core 0; no IRQ/core-1 writer exists. */
static volatile runtime_snapshot_t baseline;
static volatile runtime_snapshot_t events[C64_RUNTIME_DIAG_EVENT_CAPACITY];
static volatile uint32_t sequence, next_event, event_count, overwritten;
static volatile bool initialized, baseline_valid;
static const volatile uint16_t *devinfo_ptr;

void c64_runtime_diag_init(void) {
    if (initialized) return;
    /* ROM_DATA_FLASH_DEVINFO16_PTR identifies a pointer literal in ROM,
     * not the 16-bit value itself (same indirection as the pinned SDK). */
    const uint16_t *const *literal =
        (const uint16_t *const *)rom_data_lookup(ROM_DATA_FLASH_DEVINFO16_PTR);
    if (literal) {
        uintptr_t address = (uintptr_t)*literal;
        bool in_sram = address >= SRAM_BASE && address <= SRAM_END - sizeof(uint16_t);
        bool in_bootram = address >= BOOTRAM_BASE && address <= BOOTRAM_END - sizeof(uint16_t);
        if (!(address & 1u) && (in_sram || in_bootram))
            devinfo_ptr = (const volatile uint16_t *)address;
    }
    initialized = true;
}

static __force_inline void read_snapshot(volatile runtime_snapshot_t *s,
                                        uint32_t seq, uint32_t event, uint32_t arg) {
    uint32_t hi, lo;
    do {
        hi = timer0_hw->timerawh;
        lo = timer0_hw->timerawl;
    } while (hi != timer0_hw->timerawh);
    s->sequence = seq;
    s->event = event;
    s->arg = arg;
    s->time_lo = lo;
    s->time_hi = hi;
    for (unsigned m = 0; m < 2; ++m) {
        s->qmi[m][0] = qmi_hw->m[m].timing;
        s->qmi[m][1] = qmi_hw->m[m].rfmt;
        s->qmi[m][2] = qmi_hw->m[m].rcmd;
        s->qmi[m][3] = qmi_hw->m[m].wfmt;
        s->qmi[m][4] = qmi_hw->m[m].wcmd;
    }
    s->xip_ctrl = xip_ctrl_hw->ctrl;
    const volatile uint32_t *boot = (const volatile uint32_t *)(uintptr_t)BOOTRAM_BASE;
    for (unsigned i = 0; i < 5; ++i) s->bootram[i] = boot[i];
    s->clocks[0] = clocks_hw->clk[clk_sys].ctrl;
    s->clocks[1] = clocks_hw->clk[clk_sys].div;
    s->clocks[2] = pll_sys_hw->prim;
    s->clocks[3] = pll_sys_hw->fbdiv_int;
    s->devinfo = devinfo_ptr ? *devinfo_ptr : UINT32_MAX;
}

void __no_inline_not_in_flash_func(c64_runtime_diag_capture)(c64_runtime_diag_event_t event,
                                                           uint32_t arg) {
    if (!initialized || event < C64_RUNTIME_DIAG_BOOT_READY || event > C64_RUNTIME_DIAG_FLASH_END)
        return;
    volatile runtime_snapshot_t *s = &events[next_event];
    read_snapshot(s, ++sequence, (uint32_t)event, arg);
    if (event == C64_RUNTIME_DIAG_BOOT_READY && !baseline_valid) {
        baseline.sequence = s->sequence;
        baseline.event = s->event;
        baseline.arg = s->arg;
        baseline.time_lo = s->time_lo;
        baseline.time_hi = s->time_hi;
        for (unsigned m = 0; m < 2; ++m)
            for (unsigned i = 0; i < 5; ++i) baseline.qmi[m][i] = s->qmi[m][i];
        baseline.xip_ctrl = s->xip_ctrl;
        for (unsigned i = 0; i < 5; ++i) baseline.bootram[i] = s->bootram[i];
        for (unsigned i = 0; i < 4; ++i) baseline.clocks[i] = s->clocks[i];
        baseline.devinfo = s->devinfo;
        baseline_valid = true;
    }
    next_event = (next_event + 1u) & (C64_RUNTIME_DIAG_EVENT_CAPACITY - 1u);
    if (event_count < C64_RUNTIME_DIAG_EVENT_CAPACITY) ++event_count;
    else if (overwritten != UINT32_MAX) ++overwritten;
}

typedef struct { char *out; size_t size, used; bool ok; } json_writer_t;

static void append(json_writer_t *w, const char *format, ...) {
    if (!w->ok) return;
    va_list args;
    va_start(args, format);
    int n = vsnprintf(w->out + w->used, w->size - w->used, format, args);
    va_end(args);
    if (n < 0 || (size_t)n >= w->size - w->used) w->ok = false;
    else w->used += (size_t)n;
}

static void write_words(json_writer_t *w, const volatile uint32_t *words, unsigned count) {
    append(w, "[");
    for (unsigned i = 0; i < count; ++i)
        append(w, "%s%lu", i ? "," : "", (unsigned long)words[i]);
    append(w, "]");
}

static uint32_t changed_mask(const volatile runtime_snapshot_t *s) {
    uint32_t changed = 0;
    for (unsigned m = 0; m < 2; ++m)
        for (unsigned i = 0; i < 5; ++i)
            if (s->qmi[m][i] != baseline.qmi[m][i]) changed |= 1u << m;
    if (s->xip_ctrl != baseline.xip_ctrl) changed |= 1u << 2;
    for (unsigned i = 0; i < 5; ++i)
        if (s->bootram[i] != baseline.bootram[i]) changed |= 1u << 3;
    for (unsigned i = 0; i < 4; ++i)
        if (s->clocks[i] != baseline.clocks[i]) changed |= 1u << 4;
    if (s->devinfo != baseline.devinfo) changed |= 1u << 5;
    return changed;
}

static void write_snapshot(json_writer_t *w, const volatile runtime_snapshot_t *s) {
    uint64_t timestamp = ((uint64_t)s->time_hi << 32) | s->time_lo;
    append(w, "{\"sequence\":%lu,\"event\":%lu,\"arg\":%lu,\"timeUs\":%llu,\"m0\":",
           (unsigned long)s->sequence, (unsigned long)s->event,
           (unsigned long)s->arg, (unsigned long long)timestamp);
    write_words(w, s->qmi[0], 5);
    append(w, ",\"m1\":"); write_words(w, s->qmi[1], 5);
    append(w, ",\"xipCtrl\":%lu,\"bootram\":", (unsigned long)s->xip_ctrl);
    write_words(w, s->bootram, 5);
    append(w, ",\"clocks\":"); write_words(w, s->clocks, 4);
    if (s->devinfo == UINT32_MAX) append(w, ",\"devinfo\":null}");
    else append(w, ",\"devinfo\":%lu}", (unsigned long)s->devinfo);
}

static void output_error(char *out, size_t size, bool fragment) {
    const char *error = fragment ? ",\"c64_runtime\":{\"error\":\"buffer_too_small\"}" :
                                  "{\"schemaVersion\":1,\"error\":\"buffer_too_small\"}";
    if (!out || !size) return;
    size_t n = strlen(error);
    if (size > n) memcpy(out, error, n + 1);
    else if (!fragment && size >= 3) memcpy(out, "{}", 3);
    else out[0] = 0;
}

void c64_runtime_diag_json(char *out, size_t size) {
    if (!out || !size) return;
    if (size > C64_RUNTIME_DIAG_JSON_CAPACITY) size = C64_RUNTIME_DIAG_JSON_CAPACITY;
    json_writer_t w = {out, size, 0, true};
    runtime_snapshot_t live;
    read_snapshot(&live, sequence, 0, 0);
    uint32_t lcd[4];
    board_display_counters(&lcd[0], &lcd[1], &lcd[2], &lcd[3]);
    append(&w, "{\"schemaVersion\":1,\"initialized\":%s,\"configuredSysHz\":%lu,"
               "\"eventCapacity\":8,\"sequence\":%lu,\"overwritten\":%lu,"
               "\"devinfoAddress\":%lu,\"labels\":{"
               "\"events\":[\"live\",\"boot_ready\",\"menu_open_begin\",\"menu_open_end\","
               "\"menu_close_begin\",\"menu_close_end\",\"flash_begin\",\"flash_end\"],"
               "\"qmi\":[\"timing\",\"rfmt\",\"rcmd\",\"wfmt\",\"wcmd\"],"
               "\"clocks\":[\"clk_sys_ctrl\",\"clk_sys_div\",\"pll_sys_prim\",\"pll_sys_fbdiv_int\"],"
               "\"bootram\":[\"word0\",\"word1\",\"word2\",\"word3\",\"word4\"],"
               "\"lcd\":[\"late_max_us\",\"resyncs\",\"missed_frames\",\"late_frames\"]},\"baseline\":",
           initialized ? "true" : "false", (unsigned long)clock_get_hz(clk_sys),
           (unsigned long)sequence, (unsigned long)overwritten,
           (unsigned long)(uintptr_t)devinfo_ptr);
    if (baseline_valid) write_snapshot(&w, &baseline);
    else append(&w, "null");
    append(&w, ",\"events\":[");
    unsigned first = (next_event + C64_RUNTIME_DIAG_EVENT_CAPACITY - event_count) &
                     (C64_RUNTIME_DIAG_EVENT_CAPACITY - 1u);
    for (unsigned i = 0; i < event_count; ++i) {
        if (i) append(&w, ",");
        write_snapshot(&w, &events[(first + i) & (C64_RUNTIME_DIAG_EVENT_CAPACITY - 1u)]);
    }
    append(&w, "],\"live\":"); write_snapshot(&w, &live);
    append(&w, ",\"flashWrites\":%lu,\"lcd\":", (unsigned long)mem_flash_writes());
    write_words(&w, lcd, 4);
    append(&w, "}");
    if (!w.ok) output_error(out, size, false);
}

void c64_runtime_diag_summary_json(char *out, size_t size) {
    if (!out || !size) return;
    json_writer_t w = {out, size, 0, true};
    runtime_snapshot_t live;
    read_snapshot(&live, sequence, 0, 0);
    uint32_t lcd[4];
    board_display_counters(&lcd[0], &lcd[1], &lcd[2], &lcd[3]);
    unsigned last = (next_event + C64_RUNTIME_DIAG_EVENT_CAPACITY - 1u) &
                    (C64_RUNTIME_DIAG_EVENT_CAPACITY - 1u);
    append(&w, ",\"c64_runtime\":{\"schemaVersion\":1,\"initialized\":%s,\"baseline\":%s,"
               "\"events\":%lu,\"sequence\":%lu,\"overwritten\":%lu,\"lastEvent\":%lu,\"changed\":",
           initialized ? "true" : "false", baseline_valid ? "true" : "false",
           (unsigned long)event_count, (unsigned long)sequence, (unsigned long)overwritten,
           (unsigned long)(event_count ? events[last].event : 0));
    if (baseline_valid) append(&w, "%lu", (unsigned long)changed_mask(&live));
    else append(&w, "null");
    append(&w, ",\"flashWrites\":%lu,\"lcd\":", (unsigned long)mem_flash_writes());
    write_words(&w, lcd, 4);
    append(&w, ",\"endpoint\":\"/api/c64-runtime\"}");
    if (!w.ok) output_error(out, size, true);
}
#endif
