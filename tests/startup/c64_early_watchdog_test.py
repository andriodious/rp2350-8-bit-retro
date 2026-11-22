#!/usr/bin/env python3
"""Execute C64 startup and launcher decisions with a virtual hardware watchdog.

Actual startup/phase/recovery code is compiled from main.cpp. A virtual clk_ref
timer advances inside each hardware stub, including PSRAM with interrupts off.
Injected hangs test software timeout coverage, not a reproduction of the board's
electrical reset, flash, PSRAM or bus failure. No device or firmware build needed.
"""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
source = (root / 'src/machines/c64/main.cpp').read_text()


def function(text, signature):
    start = text.index(signature)
    opening = text.index('{', start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]


startup = '\n'.join(function(source, signature) for signature in (
    'static void phase(', 'extern "C" void c64_network_phase(',
    'static void startup_phase(', 'static void c64_startup_init('))
main_prefix = source.split('int main(){', 1)[1].split('    memset(keys,255,8);', 1)[0]
launcher = function((root / 'src/launcher/launcher.c').read_text(), 'int main(void)')
launcher = launcher.replace('int main(void)', 'void launcher_main(void)')

prefix = r'''
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include "system.h"
static constexpr uint32_t DIAG_MAGIC = C64_DIAG_MAGIC;
static constexpr uint32_t WATCHDOG_NON_REBOOT_MAGIC = 0x6ab73121u;
static bool recovery, fault_seen;
static uint32_t previous_phase, previous_pc, previous_cfsr;
static struct { uint32_t scratch[8]; } hw;
#define watchdog_hw (&hw)
static unsigned reset_reason, hang_stage, calls, selected_slot;
static uint64_t elapsed, deadline;
static bool enabled, interrupts_enabled;
struct WatchdogReset {};
struct StillHung {};
struct RecoveryServiced {};
struct Chained {};
static bool watchdog_enable_caused_reboot() {
    return reset_reason && hw.scratch[4] == WATCHDOG_NON_REBOOT_MAGIC;
}
static void watchdog_disable() { enabled = false; }
static void watchdog_update() { if (enabled) deadline = elapsed + 8000; }
static void watchdog_enable(unsigned ms, bool debug) {
    assert(ms == 8000 && debug);
    hw.scratch[4] = WATCHDOG_NON_REBOOT_MAGIC;
    enabled = true;
    watchdog_update();
}
static void advance(unsigned ms) {
    // Peripheral tick continues even when the CPU cannot run an interrupt.
    elapsed += ms;
    if (enabled && elapsed >= deadline) {
        enabled = false;
        reset_reason = 2;
        throw WatchdogReset{};
    }
}
static void hardware_stage(unsigned n) {
    ++calls;
    if (!recovery) assert(hw.scratch[0] == (DIAG_MAGIC | n));
    if (n == hang_stage) { advance(8001); throw StillHung{}; }
    // Every individual stage fits, but total startup exceeds one timeout.
    advance(6000);
}
static void startup_diag_mark(unsigned) {}
static void slow_flash() { hardware_stage(10); }
#define VREG_VOLTAGE_1_25 125
static void vreg_set_voltage(unsigned value) { assert(value == 125); }
static void set_sys_clock_khz(unsigned khz, bool required) {
    assert(khz == 250000 && required); hardware_stage(11);
}
static void stdio_init_all() { hardware_stage(12); }
static void board_init() { hardware_stage(13); }
static void board_psram_init() {
    interrupts_enabled = false;
    hardware_stage(14);
    interrupts_enabled = true;
}
static void c64_memory_init() { hardware_stage(15); }
extern "C" void c64_network_phase(uint32_t);
static void frontend_init() { c64_network_phase(40); }
static void frontend_frame() {
    c64_network_phase(45);
    // Recovery UI stays alive longer than a normal watchdog timeout.
    advance(20000);
    throw RecoveryServiced{};
}
static void sleep_ms(unsigned ms) { advance(ms); }
static system_settings_t settings = {SYSTEM_MAGIC, SYSTEM_VERSION, "c64", 0};
#define XIP_NOCACHE_NOALLOC_BASE ((uintptr_t)&settings)
#define BOARD_FLASH_SYSTEM_OFFSET 0
static uint8_t s_work[4096];
static void rom_load_partition_table(void*, unsigned, bool) {}
static void chain(int slot) { selected_slot = slot; throw Chained{}; }
static void reset_usb_boot(unsigned, unsigned) { assert(false); }
'''

tests = r'''
static void reset_model() {
    memset(&hw, 0, sizeof hw);
    for (unsigned i=4; i<8; ++i) hw.scratch[i]=0x12340000u+i;
    recovery = fault_seen = enabled = false;
    previous_phase = previous_pc = previous_cfsr = 0;
    reset_reason = hang_stage = calls = selected_slot = 0;
    elapsed = deadline = 0;
    interrupts_enabled = true;
}
static void assert_sdk_scratch() {
    // SDK watchdog_enable owns scratch 4; startup must leave 5..7 alone.
    for (unsigned i=5; i<8; ++i) assert(hw.scratch[i] == 0x12340000u+i);
}
static void check_launcher(unsigned slot) {
    try { launcher_main(); assert(false); } catch (const Chained&) {}
    assert(selected_slot == slot);
    assert_sdk_scratch();
}
static void set_previous_failure() {
    hw.scratch[0] = DIAG_MAGIC | 6 | 256;
    hw.scratch[1] = 0x8200;
    hw.scratch[2] = 0x200007d4;
    hw.scratch[3] = 0xe000ed38;
    hw.scratch[4] = WATCHDOG_NON_REBOOT_MAGIC;
    reset_reason = 2;
}
static void assert_previous_failure() {
    assert(previous_phase == 262 && previous_pc == 0x200007d4 && previous_cfsr == 0x8200);
    assert(hw.scratch[0] == (DIAG_MAGIC | 262));
    assert(hw.scratch[1] == 0x8200 && hw.scratch[2] == 0x200007d4 && hw.scratch[3] == 0xe000ed38);
    assert_sdk_scratch();
}
int main() {
    for (unsigned stage = 10; stage <= 15; ++stage) {
        reset_model(); hang_stage = stage;
        bool reset = false, hung = false;
        try { c64_startup_prefix(); assert(false); }
        catch (const WatchdogReset&) { reset = true; }
        catch (const StillHung&) { hung = true; }
        assert(reset == bool(C64_WATCHDOG) && hung == !bool(C64_WATCHDOG));
        assert(hw.scratch[0] == (DIAG_MAGIC | stage));
        assert(hw.scratch[4] == (C64_WATCHDOG ? WATCHDOG_NON_REBOOT_MAGIC : 0x12340004u));
        if (stage == 14) assert(!interrupts_enabled);
        check_launcher(C64_WATCHDOG ? SYSTEM_SLOT_MASTER : SYSTEM_SLOT_C64);
        if (C64_WATCHDOG) assert(hw.scratch[0] == (C64_RETURN_MAGIC | stage));
    }
    reset_model();
    hw.scratch[0] = DIAG_MAGIC | 6; hw.scratch[1] = 0x8200; hw.scratch[2] = 0x1234;
    // Cold boot retains stale scratch data but must not misdiagnose a failure.
    c64_startup_prefix();
    assert(!recovery && calls == 6 && elapsed == 36000 && enabled == bool(C64_WATCHDOG));
    assert(hw.scratch[0] == (DIAG_MAGIC | 1));
    assert(hw.scratch[1] == 0 && hw.scratch[2] == 0 && hw.scratch[3] == 0);
    // Model the existing boot banner's cooperative feeds during its DHCP wait.
    for (unsigned i=0; i<30000; i+=20) { watchdog_update(); advance(20); }
    assert(!reset_reason);
    check_launcher(SYSTEM_SLOT_C64);
    // Startup hands an armed watchdog to display preparation (phase 1).
    bool display_reset=false;
    try { advance(8001); } catch (const WatchdogReset&) { display_reset=true; }
    assert(display_reset == bool(C64_WATCHDOG));
    assert(hw.scratch[0] == (DIAG_MAGIC | 1));
    check_launcher(C64_WATCHDOG ? SYSTEM_SLOT_MASTER : SYSTEM_SLOT_C64);
    if (C64_WATCHDOG) assert(hw.scratch[0] == (C64_RETURN_MAGIC | 1));
    // A software restart marker is not an enabled-watchdog failure.
    reset_model(); reset_reason=1; hw.scratch[0]=DIAG_MAGIC|6; hw.scratch[4]=0xb007c0d3;
    check_launcher(SYSTEM_SLOT_C64);
    c64_startup_prefix(); assert(!recovery);
    // A standalone watchdog restart preserves evidence and leaves its healthy
    // recovery UI running without a later watchdog reboot loop.
    reset_model(); set_previous_failure();
    try { c64_startup_prefix(); assert(false); } catch (const RecoveryServiced&) {}
    assert(recovery && !enabled && calls == 6 && elapsed == 56000);
    assert_previous_failure();
    // A second hardware stall during standalone recovery still times out when
    // enabled, but cannot overwrite the earlier fault with a startup marker.
    reset_model(); set_previous_failure(); hang_stage=14;
    try { c64_startup_prefix(); assert(false); }
    catch (const WatchdogReset&) { assert(C64_WATCHDOG); }
    catch (const StillHung&) { assert(!C64_WATCHDOG); }
    assert_previous_failure();
    printf("C64_WATCHDOG=%d: six early hangs, display handoff, launcher fallback, stage feeds, long wait, SDK scratch isolation and recovery evidence passed\n", C64_WATCHDOG);
}
'''

with tempfile.TemporaryDirectory(prefix='c64-startup-watchdog-') as temp:
    path = Path(temp)
    test = path / 'test.cpp'
    test.write_text(prefix + startup + '\nstatic int c64_startup_prefix(){' +
                    main_prefix + '\nreturn 0;\n}\n' + launcher + tests)
    for enabled in (0, 1):
        binary = path / f'test-{enabled}'
        subprocess.run(['c++', '-std=c++17', '-g', '-fsanitize=address,undefined',
                        f'-DC64_WATCHDOG={enabled}', '-DSTARTUP_DIAG=0',
                        '-DLAUNCHER_BOOT_REPORT=0', '-I' + str(root / 'src/common'),
                        str(test), '-o', str(binary)], check=True)
        subprocess.run([str(binary)], check=True)
