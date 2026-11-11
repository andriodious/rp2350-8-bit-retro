#pragma once
#include <stdbool.h>
void boot_banner_show(void);

void boot_status_frame(void);
bool boot_status_visible(void);

// Flash hooks retained for callers; runtime status scanout uses SRAM.
void boot_status_flash_begin(void);
void boot_status_flash_end(void);
