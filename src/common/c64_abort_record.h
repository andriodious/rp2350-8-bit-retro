#ifndef C64_ABORT_RECORD_H
#define C64_ABORT_RECORD_H
#include <stdint.h>
// Only scratch 0..3 are ours. 4..7 belong to SDK boot/reset control.
// scratch1: magic, terminal kind, frontend stage, optional assertion line.
#define C64_ABORT_RECORD_MAGIC 0xad000000u
static inline uint32_t c64_abort_pack(unsigned kind,unsigned stage,unsigned line) {
    return C64_ABORT_RECORD_MAGIC | ((kind&15u)<<20) | ((stage&15u)<<16) | (line&65535u);
}
static inline int c64_abort_valid(uint32_t value) {return (value&0xff000000u)==C64_ABORT_RECORD_MAGIC;}
#if defined(C64_ABORT_DIAG) && C64_ABORT_DIAG
void c64_abort_stage(unsigned stage);
#else
static inline void c64_abort_stage(unsigned stage) {(void)stage;}
#endif
#endif
