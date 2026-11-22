/* Host checks include the complete production audio driver, with only hardware
 * calls mocked. They cannot model RP2350 bus/IRQ latency or analogue output. */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hardware/dma.h"
#include "hardware/pwm.h"
#include "hardware/irq.h"
#include "hardware/clocks.h"
#ifndef AUDIO_SOURCE
#define AUDIO_SOURCE "../../../src/machines/c64/audio.c"
#endif
#include AUDIO_SOURCE

static unsigned mock_clock=250000000, numerator, denominator, channel_claims;
static unsigned ack_count[4], addr_count[4], abort_count;
static bool pending[4], irq_enabled, channel_irq[4], busy[4], pwm_enabled;
static dma_channel_config configured[4];
static const void *read_address[4];
static volatile void *write_address[4];
static unsigned transfer_count[4];
static void (*irq_handler)(void);
static dma_hw_t dma_regs;
dma_hw_t *dma_hw=&dma_regs;
static pwm_hw_t pwm_regs;
pwm_hw_t *pwm_hw=&pwm_regs;

void gpio_set_function(unsigned p,unsigned f){assert(p==43&&f==GPIO_FUNC_PWM);}
void gpio_init(unsigned p){assert(p==43);}
void gpio_put(unsigned p,unsigned v){assert(p==43&&v==0);}
void gpio_set_dir(unsigned p,unsigned v){assert(p==43&&v==GPIO_OUT);}
unsigned pwm_gpio_to_slice_num(unsigned p){assert(p==43);return 5;}
pwm_config pwm_get_default_config(void){return (pwm_config){0};}
void pwm_config_set_wrap(pwm_config *c,unsigned w){c->wrap=w;}
void pwm_init(unsigned s,const pwm_config *c,bool e){assert(s==5&&c->wrap==1023);pwm_enabled=e;}
void pwm_set_enabled(unsigned s,bool e){assert(s==5);pwm_enabled=e;}
unsigned clock_get_hz(unsigned c){assert(c==clk_sys);return mock_clock;}
int dma_claim_unused_timer(bool r){assert(r);return 1;}
void dma_timer_set_fraction(unsigned t,unsigned n,unsigned d){assert(t==1&&n<=65535&&d>0&&d<=65535);numerator=n;denominator=d;}
int dma_claim_unused_channel(bool r){assert(r&&channel_claims<2);return 2+channel_claims++;}
dma_channel_config dma_channel_get_default_config(unsigned ch){assert(ch==2||ch==3);return (dma_channel_config){0};}
void channel_config_set_transfer_data_size(dma_channel_config *c,unsigned v){c->size=v;}
void channel_config_set_read_increment(dma_channel_config *c,bool v){c->read_inc=v;}
void channel_config_set_ring(dma_channel_config *c,bool w,unsigned b){c->ring_write=w;c->ring_bits=b;}
void channel_config_set_write_increment(dma_channel_config *c,bool v){c->write_inc=v;}
void channel_config_set_dreq(dma_channel_config *c,unsigned v){c->dreq=v;}
void channel_config_set_chain_to(dma_channel_config *c,unsigned v){c->chain=v;}
unsigned dma_get_timer_dreq(unsigned t){assert(t==1);return 60;}
void dma_channel_configure(unsigned ch,const dma_channel_config *c,volatile void *w,const void *r,unsigned count,bool trigger){
 assert(ch==2||ch==3);assert(!trigger);configured[ch]=*c;write_address[ch]=w;read_address[ch]=r;transfer_count[ch]=count;dma_hw->ch[ch].ctrl_trig=1;
}
void dma_irqn_set_channel_enabled(unsigned i,unsigned ch,bool e){assert(i==1&&(ch==2||ch==3));channel_irq[ch]=e;}
void dma_channel_start(unsigned ch){assert(ch==2);assert(irq_enabled&&channel_irq[2]&&channel_irq[3]);busy[ch]=true;}
bool dma_irqn_get_channel_status(unsigned i,unsigned ch){assert(i==1&&(ch==2||ch==3));return pending[ch];}
void dma_irqn_acknowledge_channel(unsigned i,unsigned ch){assert(i==1&&(ch==2||ch==3));pending[ch]=false;ack_count[ch]++;}
void dma_channel_set_read_addr(unsigned ch,const void *r,bool trigger){assert(!trigger&&r==buffers[ch-2]);read_address[ch]=r;addr_count[ch]++;}
void dma_channel_abort(unsigned ch){assert(ch==2||ch==3);assert(!irq_enabled&&!channel_irq[2]&&!channel_irq[3]);assert(numerator==0);assert(!(dma_hw->ch[2].ctrl_trig&1)&&!(dma_hw->ch[3].ctrl_trig&1));busy[ch]=false;abort_count++;}
bool dma_channel_is_busy(unsigned ch){assert(ch==2||ch==3);return busy[ch];}
void hw_clear_bits(uint32_t *p,uint32_t mask){*p&=~mask;}
void irq_set_exclusive_handler(unsigned i,void (*f)(void)){assert(i==1);irq_handler=f;}
void irq_set_enabled(unsigned i,bool e){assert(i==1);irq_enabled=e;}
uint32_t c64_sid_state_size(void){return 1476;}
bool c64_sid_state_in_sram(void){return true;}

static void reset_audio(void){
 memset(ring,0,sizeof ring);memset(buffers,0,sizeof buffers);head=tail=0;
 produced=blocks=underruns=tone_remaining=tone_phase=0;enabled=false;volume=50;status_mute=false;
 sid_min=32767;sid_max=-32768;pwm_min=1023;pwm_max=0;initialized=false;
 channel_claims=abort_count=0;irq_enabled=false;irq_handler=NULL;
 memset(pending,0,sizeof pending);memset(channel_irq,0,sizeof channel_irq);memset(busy,0,sizeof busy);
 memset(ack_count,0,sizeof ack_count);memset(addr_count,0,sizeof addr_count);memset(&dma_regs,0,sizeof dma_regs);
#if WEBDIAG
 stop_requested=dma_stopped=false;
#endif
}

static unsigned expected_level(int sample,unsigned percent,bool audible,bool test){
 int64_t v=512;
 if(audible) v+=(int64_t)sample*(test?50:percent)*(test?1:8)/6400;
 if(v<0)v=0;if(v>1023)v=1023;return (unsigned)v;
}
static void check_word(uint32_t word,unsigned expected){assert((word&65535)==expected);assert((word>>16)==expected);}

static void test_pcm(void){
 reset_audio();audio_set(1,200);assert(volume==200);uint32_t out[B];
 /* Every signed16 sample and legal volume; includes negative truncation edges. */
 for(unsigned v=0;v<=200;v++){
  audio_set(-1,(int)v);
  for(int first=-32768;first<=32767;first+=B){
   head=tail=0;for(unsigned i=0;i<B;i++)sid_add_sample(first+i,first+i);
   fill(out);assert(tail==B&&head==B);
   for(unsigned i=0;i<B;i++)check_word(out[i],expected_level(first+(int)i,v,true,false));
  }
 }
 assert(underruns==0);
 /* Stereo average must truncate toward zero; test both saturated signs. */
 head=tail=0;sid_add_sample(-32768,32767);sid_add_sample(32767,-32768);sid_add_sample(32767,32767);
 sid_add_sample(-32768,-32768);assert(ring[0]==0&&ring[1]==0&&ring[2]==32767&&ring[3]==-32768);
 audio_set(0,-1);fill(out);for(unsigned i=0;i<B;i++)check_word(out[i],512);
 assert(underruns==B-4);
 audio_set(1,100);audio_status_mute(true);head=tail=0;
 for(unsigned i=0;i<B;i++)sid_add_sample(30000,30000);
 fill(out);for(unsigned i=0;i<B;i++)check_word(out[i],512);assert(head==tail);
 /* Tone still drains input; mute overrides both normal SID and speaker test. */
 audio_set(0,-1);tone_remaining=300;tone_phase=0;head=tail=0;audio_status_mute(false);
 fill(out);for(unsigned i=0;i<B;i++)check_word(out[i],expected_level((i%44)<22?24000:-24000,0,true,true));
 assert(tone_remaining==44);fill(out);assert(tone_remaining==0);
 for(unsigned i=44;i<B;i++)check_word(out[i],512);
 audio_status_mute(true);tone_remaining=B;fill(out);for(unsigned i=0;i<B;i++)check_word(out[i],512);
 audio_set(-1,999);assert(volume==200);audio_set(-1,-1);assert(volume==200);audio_set(-1,0);assert(volume==0);
}

static void test_ring(void){
 reset_audio();audio_set(1,1);head=tail=UINT32_MAX-127u;uint32_t start=head;
 for(unsigned i=0;i<N;i++)sid_add_sample((int)i,(int)i);
 assert(head-start==N&&sid_get_buffer_fill()==N);
 int16_t saved[N];memcpy(saved,ring,sizeof ring);
 for(unsigned i=0;i<100;i++)sid_add_sample(-12345,123);
 assert(produced==N+100&&head-start==N&&!memcmp(saved,ring,sizeof ring));
 uint32_t out[B];
 for(unsigned b=0;b<N/B;b++){
  fill(out);for(unsigned i=0;i<B;i++)check_word(out[i],expected_level(b*B+i,1,true,false));
 }
 assert(head==tail&&underruns==0);fill(out);assert(underruns==B);
 for(unsigned i=0;i<B;i++)check_word(out[i],512);
 assert(audio_ring_size()==N*sizeof(int16_t));
}

static void test_init_and_irq(void){
 reset_audio();audio_init();assert(initialized&&pwm_enabled&&irq_handler==irq_audio&&busy[2]);
 assert(underruns==2*B); /* Boot-time silence is counted although nothing was audible. */
 for(unsigned i=0;i<2;i++){
  unsigned ch=channels[i];assert(ch==2+i);
  assert(configured[ch].size==DMA_SIZE_32&&configured[ch].read_inc&&!configured[ch].write_inc);
  assert(configured[ch].ring_bits==10&&!configured[ch].ring_write&&configured[ch].chain==(ch^1));
  assert(configured[ch].dreq==60&&transfer_count[ch]==B&&read_address[ch]==buffers[i]);
  assert(write_address[ch]==&pwm_hw->slice[5].cc&&((uintptr_t)buffers[i]&1023)==0);
 }
 double rate=(double)mock_clock*numerator/denominator;
 assert(numerator>0&&rate>=44090&&rate<=44110);
 for(unsigned i=0;i<2*B;i++)sid_add_sample(1000,1000);
 pending[2]=pending[3]=true;irq_handler();assert(blocks==2&&head==tail&&underruns==2*B);
 assert(ack_count[2]==1&&ack_count[3]==1&&addr_count[2]==1&&addr_count[3]==1);
 irq_handler();assert(blocks==2);
 char json[1024];c64_audio_json(json,sizeof json);assert(strstr(json,"\"ready\":true")&&strstr(json,"\"underruns\":512"));
#if WEBDIAG
 c64_audio_stop_request();c64_audio_diag_poll();assert(dma_stopped&&abort_count==2&&!pwm_enabled);
 c64_audio_diag_poll();assert(abort_count==2);
#endif
}

typedef struct { uint32_t missing, generated, drained, remaining; } schedule_result;
/* Events reproduce producer bursts and DMA block requests. They deliberately
 * exclude interrupts, LCD, flash and scheduling overhead, which need hardware. */
static schedule_result schedule(double output_rate,unsigned burst_us,unsigned nominal_rate,unsigned seconds,unsigned prefill){
 reset_audio();audio_set(1,50);if(!nominal_rate)audio_status_mute(true);
 uint32_t out[B];for(unsigned i=0;i<prefill;i++)sid_add_sample(1000,1000);
 unsigned generated_prefill=produced, block_count=0;uint64_t synthesis_fraction=0,producer_count=0;
 double next_drain=(double)B/output_rate, next_produce=(double)burst_us/1000000.0;
 while(next_drain<=seconds||next_produce<=seconds){
  if(next_produce<=next_drain&&next_produce<=seconds){
   synthesis_fraction+=(uint64_t)nominal_rate*burst_us;
   unsigned count=synthesis_fraction/1000000;synthesis_fraction%=1000000;
   for(unsigned i=0;i<count;i++)sid_add_sample(1000,1000);
   producer_count++;next_produce=(double)((producer_count+1)*burst_us)/1000000.0;
  }else if(next_drain<=seconds){fill(out);block_count++;next_drain=(double)(block_count+1)*B/output_rate;}
  else break;
  assert(head-tail<=N);
 }
 schedule_result r={underruns,produced-generated_prefill,block_count*B,head-tail};
 assert((uint32_t)(head-tail)==r.remaining);return r;
}

/* The actual SID fixed-point production rate is slightly below nominal44100:
 * 185265/65536 samples per raster line,312 lines/frame,50 frames/second.
 * Preserve the line accumulator across frames. Timing is deliberately modeled
 * as a complete frame's production every20ms,not real CPU/IRQ interleaving. */
static schedule_result sid_schedule(double output_rate,unsigned seconds,unsigned prefill){
 reset_audio();audio_set(1,50);uint32_t out[B];
 for(unsigned i=0;i<prefill;i++)sid_add_sample(1000,1000);
 unsigned generated_prefill=produced,block_count=0,frames=0;
 uint32_t fraction=0;double next_drain=(double)B/output_rate,next_frame=0.020;
 while(next_drain<=seconds||next_frame<=seconds){
  if(next_frame<=next_drain&&next_frame<=seconds){
   for(unsigned line=0;line<312;line++){
    fraction+=185265;unsigned count=fraction>>16;fraction&=65535;
    for(unsigned sample=0;sample<count;sample++)sid_add_sample(1000,1000);
   }
   frames++;next_frame=(double)(frames+1)/50.0;
  }else if(next_drain<=seconds){fill(out);block_count++;next_drain=(double)(block_count+1)*B/output_rate;}
  else break;
  assert(head-tail<=N);
 }
 assert(frames==seconds*50);
 assert(produced-generated_prefill==((uint64_t)frames*312*185265)/65536);
 return (schedule_result){underruns,produced-generated_prefill,block_count*B,head-tail};
}

static void print_schedule(const char *name,schedule_result r){printf("\"%s\":{\"missing_samples\":%u,\"produced\":%u,\"requested\":%u,\"remaining\":%u}",name,r.missing,r.generated,r.drained,r.remaining);}

typedef struct { uint32_t before, paused, after, ring_after; } pause_result;
static pause_result pause_resume(double rate){
 reset_audio();audio_set(1,50);uint32_t out[B],before=0,paused=0,drains=0;
 uint64_t fraction=0;double next_drain=(double)B/rate;
 for(unsigned ms=1;ms<=22000;ms++){
  bool pause=ms>1000&&ms<=21000;audio_status_mute(pause);
  if(!pause){fraction+=44100;unsigned count=fraction/1000;fraction%=1000;
   for(unsigned i=0;i<count;i++)sid_add_sample(1000,1000);}
  while(next_drain<=(double)ms/1000){
   fill(out);drains++;next_drain=(double)(drains+1)*B/rate;
   if(pause)for(unsigned i=0;i<B;i++)check_word(out[i],512);
  }
  if(ms==1000)before=underruns;if(ms==21000)paused=underruns;
 }
 assert(paused-before>20*44000-N);assert(underruns-paused<=2*B);
 return (pause_result){before,paused,underruns,head-tail};
}

#if HAS_AUDIO_CLOCK_FRACTION
static void test_clock_fraction(void){
 /* Independent exhaustive denominator search: for each denominator only the
  * two nearest numerators can be optimal. Exact integer comparison avoids
  * rounding a slightly worse fraction into a pass. */
 const uint32_t clocks[]={44100,44101,50000,65535,65536,1000000,12000000,
  48000000,125000000,133000000,150000000,200000000,250000000,252000000,
  266000000,300000000,378000000,400000000,504000000,1000000000,UINT32_MAX};
 for(unsigned k=0;k<sizeof(clocks)/sizeof(*clocks);k++){
  uint32_t hz=clocks[k];uint16_t n,d;audio_clock_fraction(hz,&n,&d);
  assert(n&&d);uint64_t actual=(uint64_t)hz*n,wanted=(uint64_t)44100*d;
  uint64_t actual_error=actual>wanted?actual-wanted:wanted-actual;
  for(uint32_t trial_d=1;trial_d<=65535;trial_d++){
   uint64_t target=(uint64_t)44100*trial_d;uint32_t floor_n=target/hz;
   for(unsigned next=0;next<2;next++){
    uint32_t trial_n=floor_n+next;if(trial_n<1)trial_n=1;if(trial_n>65535)trial_n=65535;
    uint64_t sample=(uint64_t)hz*trial_n;
    uint64_t error=sample>target?sample-target:target-sample;
    assert(actual_error*trial_d<=error*d);
   }
  }
  if(hz==250000000){assert(n==11&&d==62358);}
 }
}
#endif
int main(void){
 test_pcm();test_ring();test_init_and_irq();
#if HAS_AUDIO_CLOCK_FRACTION
 test_clock_fraction();
#endif
 reset_audio();audio_init();unsigned num=numerator,den=denominator;double rate=(double)mock_clock*num/den;
 schedule_result smooth=schedule(rate,1000,44100,600,1024);
 schedule_result bursts=schedule(rate,20000,44100,600,1024);
 schedule_result reference=schedule(44100,20000,44100,600,1024);
 schedule_result long_run=schedule(rate,20000,44100,7200,1024);
 schedule_result slow=schedule(rate,20000,22932,10,N);
 schedule_result muted=schedule(rate,1000,0,1,0);
 schedule_result sid600_empty=sid_schedule(rate,600,0);
 schedule_result sid600_prefill=sid_schedule(rate,600,1024);
 schedule_result sid7200_empty=sid_schedule(rate,7200,0);
 schedule_result sid7200_prefill=sid_schedule(rate,7200,1024);
 pause_result pause=pause_resume(rate);
 assert(reference.missing==0);assert(muted.missing>=44000&&muted.missing<=44200);
 assert(slow.missing>200000); /* A sustained26fps producer cannot feed44.1kHz DMA. */
 printf("{\"checks\":\"pass\",\"fraction_oracle_clocks\":%u,\"clock_hz\":%u,\"timer_numerator\":%u,\"timer_denominator\":%u,\"output_rate_hz\":%.9f,\"rate_error_samples_per_second\":%.9f,\"schedule_duration_seconds\":600,",HAS_AUDIO_CLOCK_FRACTION?21:0,mock_clock,num,den,rate,rate-44100);
 print_schedule("ideal_44100_smooth_1ms",smooth);printf(",");print_schedule("ideal_44100_pal_20ms_bursts",bursts);printf(",");print_schedule("ideal_44100_exact_rate_reference",reference);printf(",");
 print_schedule("ideal_44100_pal_20ms_bursts_7200seconds",long_run);printf(",");
 printf("\"sid_raster_production_rate_hz\":%.12f,",185265.0*312*50/65536);
 print_schedule("sid_raster_pal_600seconds_prefill0",sid600_empty);printf(",");
 print_schedule("sid_raster_pal_600seconds_prefill1024",sid600_prefill);printf(",");
 print_schedule("sid_raster_pal_7200seconds_prefill0",sid7200_empty);printf(",");
 print_schedule("sid_raster_pal_7200seconds_prefill1024",sid7200_prefill);printf(",");
 print_schedule("slow_26fps_10seconds",slow);printf(",");print_schedule("no_producer_muted_1second",muted);
 printf(",\"status_mute_pause_20seconds\":{\"underruns_before\":%u,\"underruns_after_pause\":%u,\"underruns_one_second_after_resume\":%u,\"ring_after_resume\":%u}}\n",pause.before,pause.paused,pause.after,pause.ring_after);
 return 0;
}
