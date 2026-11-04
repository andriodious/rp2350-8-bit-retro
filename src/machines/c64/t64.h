#ifndef PRESTO_T64_H
#define PRESTO_T64_H
#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
// Bounded, read-only program containers; no cassette pulse or fast-loader emulation.
#define T64_MAX_PROGRAMS 64
struct t64_archive { const uint8_t *data; uint32_t size; uint16_t slots, count; };
struct t64_entry { uint32_t offset, length; uint16_t start; bool runnable; char name[17]; };
const char *t64_open(struct t64_archive *out, const uint8_t *data, uint32_t size);
bool t64_entry_get(const struct t64_archive *archive, unsigned index, struct t64_entry *out);
bool t64_load(const struct t64_archive *archive, unsigned index, uint8_t *ram);
// Firmware integration, called only on core 0 between emulation frames.
bool c64_tape_insert(const uint8_t *image, uint32_t size);
unsigned c64_tape_count(void);
uint32_t c64_tape_generation(void);
bool c64_tape_entry(unsigned index, struct t64_entry *out);
bool c64_tape_select(unsigned index, bool run);
void c64_tape_status(char *out, uint32_t size);
#ifdef __cplusplus
}
#endif
#endif
