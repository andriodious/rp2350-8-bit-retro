// Isolated diagnostic: protect only core-0 emulation calls. SDK watchdog reboot
// uses scratch[4..7]; retain our evidence in scratch[0..3]. No flash writes.
#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/exception.h"
#include "hardware/sync.h"
#include "hardware/watchdog.h"
#include "hardware/structs/watchdog.h"
#include "hardware/structs/scb.h"
#include "hang_diag.h"
#define RUNNING 0x53504558u
#define FAULTED 0x53504654u
static bool recovery;
static uint32_t reset_reason;
void __attribute__((used,noinline)) spectrum_diag_fault(uint32_t *frame, uint32_t exc_return) {
    __asm volatile("cpsid i");
    uint32_t cfsr=scb_hw->cfsr, pc=0;
    // FP state follows the basic frame. A security-transition callee frame,
    // when EXC_RETURN.DCRS is clear, precedes it by ten words.
    uintptr_t basic=(uintptr_t)frame+((exc_return & 32u)?0:10u*4u);
    // Do not dereference a failed exception-stacking frame.
    if (!(cfsr & 0x3838u) && !(basic & 3u) && basic>=0x20000000u && basic<=0x20081fe0u)
        pc=((uint32_t *)basic)[6];
    watchdog_hw->scratch[1]=pc;
    watchdog_hw->scratch[2]=cfsr;
    watchdog_hw->scratch[3]=scb_hw->bfar;
    __dmb();
    watchdog_hw->scratch[0]=FAULTED;
    watchdog_reboot(0,0,10);
    for(;;)__wfi();
}
static void __attribute__((naked)) capture_fault(void) {
    __asm volatile("tst lr, #4\n"
                   "ite eq\n"
                   "mrseq r0, msp\n"
                   "mrsne r0, psp\n"
                   "mov r1, lr\n"
                   "b spectrum_diag_fault\n");
}
void spectrum_diag_init(void) {
    uint32_t tag=watchdog_hw->scratch[0];
    recovery=(tag==RUNNING || tag==FAULTED);
    reset_reason=watchdog_hw->reason;
    exception_set_exclusive_handler(HARDFAULT_EXCEPTION,capture_fault);
}
bool spectrum_diag_recovery(void) {return recovery;}
void spectrum_diag_begin(void) {
    watchdog_hw->scratch[1]=watchdog_hw->scratch[2]=watchdog_hw->scratch[3]=0;
    watchdog_hw->scratch[0]=RUNNING;
    __dmb();
    watchdog_enable(2000,true);
}
void spectrum_diag_end(void) {
    watchdog_disable();
    __dmb();
    watchdog_hw->scratch[0]=0;
}
void spectrum_diag_status(char *out,uint32_t size) {
    snprintf(out,size,",\"spectrum_hang_diag\":{\"version\":2,\"recovery\":%s,\"kind\":\"%s\",\"pc\":\"%08lx\",\"cfsr\":\"%08lx\",\"bfar\":\"%08lx\",\"reset_reason\":%lu}",
        recovery?"true":"false",!recovery?"none":watchdog_hw->scratch[0]==FAULTED?"hardfault":"interrupted_execution",
        (unsigned long)watchdog_hw->scratch[1],(unsigned long)watchdog_hw->scratch[2],
        (unsigned long)watchdog_hw->scratch[3],(unsigned long)reset_reason);
}
