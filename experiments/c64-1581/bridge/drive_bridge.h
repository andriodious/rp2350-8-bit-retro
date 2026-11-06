// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <stdint.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
bool retro_drive_init(const uint8_t *,size_t,const uint8_t *,size_t);
void retro_drive_tick(uint32_t);
void retro_drive_flush(void);
uint8_t retro_drive_read(void);
void retro_drive_write(uint8_t);
uint8_t retro_drive_peek(uint16_t);
uint16_t retro_drive_pc(void);
#ifdef __cplusplus
}
#endif
