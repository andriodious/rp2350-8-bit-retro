// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
bool retro_drive_init(const uint8_t *,size_t,const uint8_t *,size_t);
bool retro_drive_active(void);
// Core-0-only, flushed on every scanline and before every CIA2 serial access.
extern bool retro_drive_enabled;
extern uint32_t retro_drive_pending;
static inline void retro_drive_add_cycles(uint32_t cycles) {
    if (retro_drive_enabled) retro_drive_pending += cycles;
}
void retro_drive_close(void);
void retro_drive_reset(void);
void retro_drive_tick(uint32_t);
void retro_drive_flush(void);
// At scanline boundaries only; transport polling, never API command execution.
void retro_drive_network_poll(void);
uint8_t retro_drive_read(void);
void retro_drive_write(uint8_t);
uint8_t retro_drive_peek(uint16_t);
uint16_t retro_drive_pc(void);
uint64_t retro_drive_profile(uint32_t index);
uint64_t retro_drive_run_us(void);
uint64_t retro_drive_run_calls(void);
uint64_t retro_drive_instructions(void);
uint64_t retro_drive_idle_loops(void);
uint64_t retro_drive_idle_tail_instructions(void);
uint64_t retro_drive_empty_calls(void);
uint64_t retro_drive_empty_us(void);
extern const uint8_t retro_1581_rom[];
#ifdef __cplusplus
}
#endif
