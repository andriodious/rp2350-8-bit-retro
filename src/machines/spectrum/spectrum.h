#pragma once
#include <stdint.h>
#include <stdbool.h>
void spectrum_reset(void);
void spectrum_init(uint8_t *external);
bool spectrum_load_sna(const uint8_t *data, uint32_t size);
void spectrum_exec(unsigned us);
void spectrum_key(unsigned code, bool down);
const uint8_t *spectrum_pixels(void);
uint32_t spectrum_frames(void);
uint16_t spectrum_pc(void);
void spectrum_audio(const float *samples, int count);

void spectrum_joystick(unsigned directions, bool fire);

void spectrum_render_line(unsigned y,uint8_t out[320]);
void spectrum_profile_get(uint32_t out[5]);

typedef struct {uint8_t border,blink,bits[32],attrs[32];} spectrum_line_t;
void spectrum_render_record(const spectrum_line_t *line,unsigned y,uint8_t out[320]);
void spectrum_set_line_sink(void (*sink)(unsigned,const spectrum_line_t *));

// Cumulative sampled timings, owned and read on core 0.
typedef struct {
    uint32_t events, samples, measured_us, timer_baseline_us;
} spectrum_audio_work_t;
void spectrum_audio_work_get(spectrum_audio_work_t out[2]);

bool spectrum_load_z80(const uint8_t *,uint32_t);
bool spectrum_load_image(const char *,const uint8_t *,uint32_t);
void spectrum_media_storage(uint8_t *,uint32_t);
const char *spectrum_media_error(void);
bool spectrum_tape_command(const char *);
const char *spectrum_tape_state(void);
const char *spectrum_tape_error(void);
uint32_t spectrum_tape_block(void);
uint32_t spectrum_tape_blocks(void);
