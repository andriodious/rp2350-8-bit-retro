#pragma once
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "mem.h"
#include "c64_runtime_diag.h"

#define FLASH_PAGE_SIZE 256u
#define FLASH_SECTOR_SIZE 4096u
#define BOARD_FLASH_SAVE_SLOT 0x40000u
#define BOARD_FLASH_LAYOUT { [MEM_FLASH_SYSTEM]={0xf00000u,FLASH_SECTOR_SIZE} }
#define XIP_NOCACHE_NOALLOC_NOTRANSLATE_BASE ((uintptr_t)0x1c000000u)
#define PICO_OK 0
#define __no_inline_not_in_flash_func(name) name
#define __dsb mock_dsb
#define __isb mock_isb
typedef struct { volatile uint32_t timing, rfmt, rcmd, wfmt, wcmd; } mock_window;
static struct { mock_window m[2]; } mock_qmi;
#define qmi_hw (&mock_qmi)

unsigned get_core_num(void);
bool multicore_lockout_victim_is_initialized(unsigned core);
uint32_t save_and_disable_interrupts(void);
void restore_interrupts(uint32_t state);
int flash_safe_execute(void (*callback)(void *),void *arg,uint32_t timeout);
void flash_range_erase(uint32_t offset,size_t count);
void flash_range_program(uint32_t offset,const uint8_t *data,size_t count);
void boot_status_flash_begin(void);
void boot_status_flash_end(void);
const char *machine_name(void);
bool system_launched(void);
void __dsb(void);
void __isb(void);
