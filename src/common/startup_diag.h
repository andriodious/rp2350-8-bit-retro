#pragma once
#ifdef __cplusplus
extern "C" {
#endif
#if STARTUP_DIAG
void startup_diag_mark(unsigned stage);
static inline unsigned startup_diag_pixel(unsigned stage, unsigned pixel) {
    unsigned colour = (stage & 0xf00u) == 0x100u ? 0x000008u :
                      (stage & 0xf00u) == 0x200u ? 0x040800u :
                      (stage & 0xf00u) == 0x300u ? 0x000800u :
                      (stage & 0xf00u) == 0x400u ? 0x080808u : 0x080000u;
    return pixel < (stage & 255u) ? colour << 8 : 0;
}
#else
static inline void startup_diag_mark(unsigned stage) {(void)stage;}
#endif
#ifdef __cplusplus
}
#endif
