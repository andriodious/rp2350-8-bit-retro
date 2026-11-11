#pragma once
/*
    beeper.h    -- simple square-wave beeper

    TODO: docs

    ## zlib/libpng license

    Copyright (c) 2018 Andre Weissflog
    This software is provided 'as-is', without any express or implied warranty.
    In no event will the authors be held liable for any damages arising from the
    use of this software.
    Permission is granted to anyone to use this software for any purpose,
    including commercial applications, and to alter it and redistribute it
    freely, subject to the following restrictions:
        1. The origin of this software must not be misrepresented; you must not
        claim that you wrote the original software. If you use this software in a
        product, an acknowledgment in the product documentation would be
        appreciated but is not required.
        2. Altered source versions must be plainly marked as such, and must not
        be misrepresented as being the original software.
        3. This notice may not be removed or altered from any source
        distribution.
*/
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// error-accumulation precision boost
#define BEEPER_FIXEDPOINT_SCALE (16)
// DC adjust buffer size
#define BEEPER_DCADJ_BUFLEN (128)

// initialization parameters
typedef struct {
    int tick_hz;
    int sound_hz;
    float base_volume;
} beeper_desc_t;

// beeper state
typedef struct {
    int state;
    int period;
    int counter;
    float base_volume;
    float volume;
    float sample;
    float dcadj_sum;
    uint32_t dcadj_pos;
#ifdef BEEPER_BINARY_FIXED_GAIN
    unsigned binary_sum;
#ifdef BEEPER_LAZY_HISTORY
    int history_counter;
#endif
    uint8_t binary_buf[BEEPER_DCADJ_BUFLEN];
#else
    float dcadj_buf[BEEPER_DCADJ_BUFLEN];
#endif
} beeper_t;

#ifndef BEEPER_HISTORY_BEGIN
#define BEEPER_HISTORY_BEGIN() ((void)0)
#define BEEPER_HISTORY_END() ((void)0)
#endif
#ifdef BEEPER_LAZY_HISTORY
// All pending cycles have the current level; flush before a transition/sample/reset.
static inline void beeper_flush_history(beeper_t *bp) {
    BEEPER_HISTORY_BEGIN();
    unsigned ticks = (unsigned)(bp->history_counter - bp->counter) / BEEPER_FIXEDPOINT_SCALE;
    bp->history_counter = bp->counter;
    // Once uniform, rotating the ring cannot change any future output.
    while (ticks && bp->binary_sum != (unsigned)bp->state * BEEPER_DCADJ_BUFLEN) {
        --ticks;
        bp->binary_sum -= bp->binary_buf[bp->dcadj_pos];
        bp->binary_sum += bp->state;
        bp->binary_buf[bp->dcadj_pos] = (uint8_t)bp->state;
        bp->dcadj_pos = (bp->dcadj_pos + 1) & (BEEPER_DCADJ_BUFLEN - 1);
    }
    BEEPER_HISTORY_END();
}
#endif
// initialize beeper instance
void beeper_init(beeper_t* beeper, const beeper_desc_t* desc);
// reset the beeper instance
void beeper_reset(beeper_t* beeper);
// set current on/off state
static inline void beeper_set(beeper_t* beeper, bool state) {
#ifdef BEEPER_LAZY_HISTORY
    if (beeper->state != (state ? 1 : 0)) beeper_flush_history(beeper);
#endif
    beeper->state = state ? 1 : 0;
}
// toggle current state (on->off or off->on)
static inline void beeper_toggle(beeper_t* beeper) {
#ifdef BEEPER_LAZY_HISTORY
    beeper_flush_history(beeper);
#endif
    beeper->state = !beeper->state;
}
// set current volume 0.0 to 1.0
static inline void beeper_set_volume(beeper_t* beeper, float vol) {
#ifdef BEEPER_BINARY_FIXED_GAIN
    (void)vol; // Fixed-gain mode is private to the Spectrum wrapper; gain is not adjusted here.
#endif
    beeper->volume = vol;
}
// tick the beeper, return true if a new sample is ready
#ifndef BEEPER_TICK_ATTR
#define BEEPER_TICK_ATTR
#endif
BEEPER_TICK_ATTR bool beeper_tick(beeper_t* beeper);

#ifdef __cplusplus
} /* extern "C" */
#endif
/*-- IMPLEMENTATION ----------------------------------------------------------*/
#ifdef CHIPS_IMPL
#include <string.h>
#ifndef CHIPS_ASSERT
    #include <assert.h>
    #define CHIPS_ASSERT(c) assert(c)
#endif

void beeper_init(beeper_t* b, const beeper_desc_t* desc) {
    CHIPS_ASSERT(b && desc);
    CHIPS_ASSERT((desc->tick_hz > 0) && (desc->sound_hz > 0));
#ifdef BEEPER_BINARY_FIXED_GAIN
    CHIPS_ASSERT(desc->base_volume == 0.5f);
#endif
    *b = (beeper_t){
        .period = (desc->tick_hz * BEEPER_FIXEDPOINT_SCALE) / desc->sound_hz,
        .counter = b->period,
#ifdef BEEPER_LAZY_HISTORY
        .history_counter = b->period,
#endif
        .base_volume = desc->base_volume,
        .volume = 1.0f,
    };
}

void beeper_reset(beeper_t* b) {
    CHIPS_ASSERT(b);
#ifdef BEEPER_LAZY_HISTORY
    beeper_flush_history(b);
#endif
    b->state = 0;
    b->counter = b->period;
#ifdef BEEPER_LAZY_HISTORY
    b->history_counter = b->counter;
#endif
    b->sample = 0;
}

/* DC adjustment filter from StSound, this moves an "offcenter"
   signal back to the zero-line (e.g. the volume-level output
   from the chip simulation which is >0.0 gets converted to
   a +/- sample value)
*/
#ifndef BEEPER_BINARY_FIXED_GAIN
static void _beeper_dcadjust(beeper_t* bp, float s) {
    bp->dcadj_sum -= bp->dcadj_buf[bp->dcadj_pos];
    bp->dcadj_sum += s;
    bp->dcadj_buf[bp->dcadj_pos] = s;
    bp->dcadj_pos = (bp->dcadj_pos + 1) & (BEEPER_DCADJ_BUFLEN-1);
}

#endif
BEEPER_TICK_ATTR bool beeper_tick(beeper_t* bp) {
#ifdef BEEPER_BINARY_FIXED_GAIN
#ifdef BEEPER_LAZY_HISTORY
    // History derives elapsed cycles from the sample countdown; no per-cycle work.
#else
    // Spectrum's gain is fixed at 0.5: count the exact binary history in integers.
    // Preserve the original 128-cycle box filter, sample clock and reset history.
    // A uniform window is invariant under rotation; no ring write is needed.
    if(bp->binary_sum != (unsigned)bp->state * BEEPER_DCADJ_BUFLEN) {
    bp->binary_sum -= bp->binary_buf[bp->dcadj_pos];
    bp->binary_sum += bp->state;
    bp->binary_buf[bp->dcadj_pos] = (uint8_t)bp->state;
    bp->dcadj_pos = (bp->dcadj_pos + 1) & (BEEPER_DCADJ_BUFLEN-1);
    }
#endif
#else
    _beeper_dcadjust(bp, (float)bp->state * bp->volume * bp->base_volume);
    #endif
    /* generate a new sample? */
    bp->counter -= BEEPER_FIXEDPOINT_SCALE;
    if (bp->counter <= 0) {
#ifdef BEEPER_LAZY_HISTORY
        beeper_flush_history(bp);
#endif
        bp->counter += bp->period;
#ifdef BEEPER_LAZY_HISTORY
        bp->history_counter = bp->counter;
#endif
#ifdef BEEPER_BINARY_FIXED_GAIN
        bp->sample = (float)bp->binary_sum * (0.5f / BEEPER_DCADJ_BUFLEN);
#else
        bp->sample = bp->dcadj_sum / BEEPER_DCADJ_BUFLEN;
#endif
        return true;
    }
    return false;
}


#endif /* CHIPS_IMPL */
