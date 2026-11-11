#pragma once
void boot_banner_show(void);

void boot_status_frame(void);

// Suspend PSRAM scanout while the shared flash/QMI bus is unavailable.
void boot_status_flash_begin(void);
void boot_status_flash_end(void);
