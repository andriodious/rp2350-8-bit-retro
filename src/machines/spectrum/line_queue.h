#pragma once
#include "spectrum.h"
// Single producer/core 0, single consumer/core 1. Never overwrite a live slot.
#define SPECTRUM_QUEUE_SIZE 8u
typedef struct {unsigned y;spectrum_line_t line;} spectrum_job_t;
typedef struct {unsigned head,tail;spectrum_job_t jobs[SPECTRUM_QUEUE_SIZE];} spectrum_queue_t;
static inline bool spectrum_queue_push(spectrum_queue_t *q,unsigned y,const spectrum_line_t *line){
 unsigned h=__atomic_load_n(&q->head,__ATOMIC_RELAXED);
 if(h-__atomic_load_n(&q->tail,__ATOMIC_ACQUIRE)==SPECTRUM_QUEUE_SIZE)return false;
 q->jobs[h%SPECTRUM_QUEUE_SIZE]=(spectrum_job_t){.y=y,.line=*line};
 __atomic_store_n(&q->head,h+1,__ATOMIC_RELEASE);return true;
}
static inline bool spectrum_queue_pop(spectrum_queue_t *q,spectrum_job_t *job){
 unsigned t=__atomic_load_n(&q->tail,__ATOMIC_RELAXED);
 if(t==__atomic_load_n(&q->head,__ATOMIC_ACQUIRE))return false;
 *job=q->jobs[t%SPECTRUM_QUEUE_SIZE];
 __atomic_store_n(&q->tail,t+1,__ATOMIC_RELEASE);return true;
}
