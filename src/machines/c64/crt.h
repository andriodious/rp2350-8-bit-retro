#ifndef PRESTO_CRT_H
#define PRESTO_CRT_H
#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
// Core-0, between-frame operations; failure preserves the existing cartridge.
const char *c64_crt_prepare(const uint8_t *data, uint32_t size);
void c64_crt_detach(void);
bool c64_crt_active(void);
bool c64_cartridge_insert(const uint8_t *data, uint32_t size);
void c64_cartridge_eject(void);
#ifdef __cplusplus
}
#endif
#endif
