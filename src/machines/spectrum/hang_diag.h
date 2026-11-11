#pragma once
#include <stdbool.h>
#include <stdint.h>
void spectrum_diag_init(void);
void spectrum_diag_begin(void);
void spectrum_diag_end(void);
bool spectrum_diag_recovery(void);
void spectrum_diag_status(char *out, uint32_t size);
