// Diagnostic-only terminal hooks: no allocation, printf, flash writes or lockout.
#include "pico/stdlib.h"
#include "hardware/sync.h"
#include "hardware/watchdog.h"
#include "hardware/structs/watchdog.h"
#include "system.h"
#include "c64_abort_record.h"
extern void c64_terminal_mark(void);
static volatile unsigned frontend_stage[2];
void c64_abort_stage(unsigned stage) { frontend_stage[get_core_num()]=stage; }
static void __attribute__((noreturn,noinline)) record(unsigned kind,unsigned line,uintptr_t caller,uintptr_t detail) {
    save_and_disable_interrupts();
    c64_terminal_mark();
    // Preserve an already-published terminal record.
    if (!c64_abort_valid(watchdog_hw->scratch[1])) {
        watchdog_hw->scratch[2]=(uint32_t)caller;
        watchdog_hw->scratch[3]=(uint32_t)detail;
        watchdog_hw->scratch[0]=C64_DIAG_MAGIC | (watchdog_hw->scratch[0]&255u) | (get_core_num()<<8);
        __dmb();
        watchdog_hw->scratch[1]=c64_abort_pack(kind,frontend_stage[get_core_num()],line);
        __dmb();
    }
    // Same launcher fallback as a fault, also works for an early startup abort.
    watchdog_enable(1000,true);
    for (;;) __wfi();
}
#define CALLER ((uintptr_t)__builtin_return_address(0))
void __attribute__((noreturn,noinline)) __wrap_abort(void) {record(1,0,CALLER,0);}
void __attribute__((noreturn,noinline)) __wrap_exit(int status) {record(2,0,CALLER,(uint32_t)status);}
// Strong override also catches SDK-internal references to its weak _exit.
void __attribute__((noreturn,noinline)) _exit(int status) {record(3,0,CALLER,(uint32_t)status);}
void __attribute__((noinline)) __assert_func(const char *file,int line,const char *func,const char *expr) {
    (void)file;(void)func;record(4,(unsigned)line,CALLER,(uintptr_t)expr);
}
void __attribute__((noreturn,noinline)) __wrap_panic(const char *fmt,...) {record(5,0,CALLER,(uintptr_t)fmt);}
void __attribute__((noreturn,noinline)) hard_assertion_failure(void) {record(6,0,CALLER,0);}
