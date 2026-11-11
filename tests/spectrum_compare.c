#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <assert.h>
#ifdef HOST_PROFILE
static unsigned countdown=509;static int active;static uint64_t last,parts[4];
static uint64_t now(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (uint64_t)t.tv_sec*1000000000+t.tv_nsec;}
static void sample(unsigned stage){if(stage==0){active=!--countdown;if(active){countdown=509;last=now();}}else if(active){uint64_t t=now();parts[stage-1]+=t-last;last=t;}}
#define ZX_PROFILE(stage) sample(stage)
#endif
#include "spectrum.c"
#ifdef SPECTRUM_AUDIO_PROFILE
uint32_t spectrum_test_time_us(void) { static uint32_t ticks=UINT32_MAX-5; return ticks++; }
#endif
static uint8_t ext[262144];
static uint32_t audio_hash=2166136261u;
void spectrum_audio(const float *p,int n){for(int i=0;i<n;i++){uint32_t u;memcpy(&u,p+i,4);audio_hash=(audio_hash^u)*16777619;}}
#ifdef PIPELINE_TEST
#include <pthread.h>
#include <sched.h>
#include "line_queue.h"
static spectrum_queue_t q;
static unsigned produced,consumed,stop;
static void sink(unsigned y,const spectrum_line_t *line){while(!spectrum_queue_push(&q,y,line))sched_yield();produced++;}
static void *worker(void *unused){(void)unused;for(;;){spectrum_job_t job;
 if(spectrum_queue_pop(&q,&job)){spectrum_render_record(&job.line,job.y,zx.fb+job.y*512);__atomic_fetch_add(&consumed,1,__ATOMIC_RELEASE);}
 else if(__atomic_load_n(&stop,__ATOMIC_ACQUIRE))return NULL;else sched_yield();}}
#endif
extern const uint8_t spectrum_elite[];
extern const unsigned spectrum_elite_size;
int main(void){spectrum_init(ext);assert(spectrum_load_sna(spectrum_elite,spectrum_elite_size));
#ifdef PIPELINE_TEST
 // Exercise a full queue and unsigned cursor wrap before the threaded run.
 q.head=q.tail=UINT32_MAX-3; spectrum_line_t marker={.border=7};spectrum_job_t job;
 for(unsigned i=0;i<8;i++)assert(spectrum_queue_push(&q,i,&marker));
 assert(!spectrum_queue_push(&q,9,&marker));
 for(unsigned i=0;i<8;i++){assert(spectrum_queue_pop(&q,&job));assert(job.y==i&&job.line.border==7);}
 assert(!spectrum_queue_pop(&q,&job));
 pthread_t thread;assert(!pthread_create(&thread,NULL,worker,NULL));spectrum_set_line_sink(sink);
#endif
 clock_t start=clock();
 for(int f=0;f<600;f++){
 if(f==250)spectrum_key(38,true);if(f==350)spectrum_key(38,false);
 if(f==450)spectrum_key(35,true);if(f==500)spectrum_key(35,false);
#ifdef SLICED_EXEC_TEST
 // Mix call boundaries, including single-microsecond calls, across a frame.
 // Both reference and candidate use identical slices (including tick rounding).
 unsigned left=19968,seed=(unsigned)f+1;
 while(left){seed=seed*1664525u+1013904223u;unsigned n=1+(seed%997);if(n>left)n=left;spectrum_exec(n);left-=n;}
#else
 spectrum_exec(19968);
#endif
#ifdef PIPELINE_TEST
 while(__atomic_load_n(&consumed,__ATOMIC_ACQUIRE)!=produced)sched_yield();
#endif
 uint32_t hash=2166136261u;
 for(int y=0;y<256;y++){
 uint8_t row[320];
#if defined(BASELINE) || defined(PIPELINE_TEST)
 memcpy(row,zx.fb+y*512,320);
#else
 spectrum_render_line(y,row);
#endif
 for(int x=0;x<320;x++)hash=(hash^row[x])*16777619;
 }
 printf("%d %08x %08x %04x %04x\n",f,hash,audio_hash,zx.cpu.pc,zx.cpu.sp);
 }
#ifdef PIPELINE_TEST
 __atomic_store_n(&stop,1,__ATOMIC_RELEASE);assert(!pthread_join(thread,NULL));
#endif
#ifdef HOST_PROFILE
 fprintf(stderr,"Sampled host ns CPU=%llu video=%llu bus=%llu audio=%llu (includes timer overhead)\n",(unsigned long long)parts[0],(unsigned long long)parts[1],(unsigned long long)parts[2],(unsigned long long)parts[3]);
#endif
#ifdef SPECTRUM_AUDIO_PROFILE
 spectrum_audio_work_t work[2];spectrum_audio_work_get(work);
 for(unsigned i=0;i<2;i++){
  assert(work[i].samples>0);
  assert(work[i].samples==work[i].events/127);
  assert(work[i].measured_us==work[i].samples);
  assert(work[i].timer_baseline_us==work[i].samples);
 }
#endif
 fprintf(stderr,"600 frames CPU seconds %.3f\n",(double)(clock()-start)/CLOCKS_PER_SEC);
}
