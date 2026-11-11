#pragma once
#include <stdint.h>
#include <stdbool.h>
// Core-0 owned. The caller retains immutable image storage until eject.
typedef struct {
 const uint8_t *image,*data,*pulses;
 uint32_t size,pos,block,blocks,left,remaining,bit,bits,pilot_count,pause;
 uint16_t pilot,sync1,sync2,zero,one,direct;
 uint8_t phase,half,depth,call_depth;
 bool tzx,mounted,playing,level,toggle;
 struct {uint32_t pos,block,left;} loop[8];
 struct {uint32_t pos,block,base,list,index,count;} call[8];
 const char *error;
} spectrum_tape_t;
bool spectrum_tape_validate(const uint8_t *,uint32_t,bool,uint32_t *,const char **);
void spectrum_tape_mount(spectrum_tape_t *,const uint8_t *,uint32_t,bool,uint32_t);
void spectrum_tape_rewind(spectrum_tape_t *);
void spectrum_tape_play(spectrum_tape_t *);
// Advance only to the next edge. Returns a positive duration, or stops playback.
void spectrum_tape_next(spectrum_tape_t *);
