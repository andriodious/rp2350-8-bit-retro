#include "pico/stdlib.h"
void retro_drive_panic(void) { panic("Experimental 1581 allocation or runtime failure"); }

uint32_t retro_drive_time_us(void) { return time_us_32(); }
