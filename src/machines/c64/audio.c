#include "pico/stdlib.h"
#include "hardware/dma.h"
#include "hardware/sync.h"
#include "hardware/pwm.h"
#include "hardware/irq.h"
#include "hardware/clocks.h"
#include "board_config.h"
#include "sound.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
#define N 4096u
#define B 256u
static int16_t ring[N];
static volatile uint32_t head,tail;
// DMA wraps each 1024-byte buffer even while flash operations defer its IRQ.
static uint32_t buffers[2][B] __attribute__((aligned(1024)));
static int channels[2];
static int audio_timer;
#if WEBDIAG
static bool stop_requested, dma_stopped;
#endif
static volatile bool enabled;
static volatile unsigned volume=50;
static volatile bool status_mute;
void audio_status_mute(bool mute){status_mute=mute;}
static volatile uint32_t produced, blocks, underruns, tone_remaining;
static volatile int sid_min=32767,sid_max=-32768;
static volatile unsigned pwm_min=1023,pwm_max;
static unsigned tone_phase;
static bool initialized;

void sid_add_sample(int16_t l,int16_t r){int sample=((int32_t)l+r)/2;produced++;if(sample<sid_min)sid_min=sample;if(sample>sid_max)sid_max=sample;uint32_t h=head;if(h-tail>=N)return;ring[h&(N-1)]=((int32_t)l+r)/2;__dmb();head=h+1;}
int sid_get_buffer_fill(void){return head-tail;}
static void fill(uint32_t *p){
 for(unsigned i=0;i<B;i++){
  int sample=0;uint32_t t=tail;
  if(t!=head){__dmb();sample=ring[t&(N-1)];__dmb();tail=t+1;}else underruns++;
  bool test=tone_remaining!=0;
  if(test){sample=(tone_phase++%44)<22?24000:-24000;tone_remaining--;}
  int level=((enabled||test)&&!status_mute)?512+(sample*(int)(test?50:volume)*(test?1:8)/100)/64:512;
  if(level<0)level=0;if(level>1023)level=1023;
  if((unsigned)level<pwm_min)pwm_min=level;if((unsigned)level>pwm_max)pwm_max=level;
  // One aligned word updates both PWM compare channels, without a halfword alias.
  p[i]=(uint32_t)level|((uint32_t)level<<16);
 }
}
static void irq_audio(void){for(int i=0;i<2;i++)if(dma_irqn_get_channel_status(1,channels[i])){dma_irqn_acknowledge_channel(1,channels[i]);blocks++;fill(buffers[i]);dma_channel_set_read_addr(channels[i],buffers[i],false);}}
static void audio_clock_fraction(uint32_t hz,uint16_t *numerator,uint16_t *denominator){
 // DMA accepts a 16-bit fraction of clk_sys. An integer divider of 5668 at
 // 250 MHz plays 7.27 extra samples/s, steadily emptying a full-speed SID ring.
 // Search the nearby legal denominators for each possible numerator. At the
 // normal 250 MHz clock this examines just twelve numerators, once at startup.
 const uint32_t rate=44100u,limit=65535u;
 assert(hz>=rate);
 uint32_t best_n=1,best_d=1;
 uint64_t best_error=hz-rate;
 const uint32_t max_n=((uint64_t)rate*limit+hz-1)/hz;
 for(uint32_t n=1;n<=max_n;n++){
  uint64_t scaled=(uint64_t)hz*n;
  uint32_t floor_d=scaled/rate;
  for(unsigned next=0;next<2;next++){
   uint32_t d=floor_d+next;
   if(d<1)d=1;
   if(d>limit)d=limit;
   uint64_t wanted=(uint64_t)rate*d;
   uint64_t error=scaled>wanted?scaled-wanted:wanted-scaled;
   if(error*best_d<best_error*d){best_n=n;best_d=d;best_error=error;}
  }
 }
 *numerator=(uint16_t)best_n;*denominator=(uint16_t)best_d;
}
void audio_init(void){
 unsigned pin=BOARD_AUDIO_PWM_PIN,slice=pwm_gpio_to_slice_num(pin);
 gpio_set_function(pin,GPIO_FUNC_PWM);pwm_config pc=pwm_get_default_config();pwm_config_set_wrap(&pc,1023);pwm_init(slice,&pc,true);
 int timer=dma_claim_unused_timer(true);audio_timer=timer;
 uint16_t numerator,denominator;audio_clock_fraction(clock_get_hz(clk_sys),&numerator,&denominator);
 dma_timer_set_fraction(timer,numerator,denominator);
 channels[0]=dma_claim_unused_channel(true);channels[1]=dma_claim_unused_channel(true);
 for(int i=0;i<2;i++){fill(buffers[i]);dma_channel_config c=dma_channel_get_default_config(channels[i]);channel_config_set_transfer_data_size(&c,DMA_SIZE_32);channel_config_set_read_increment(&c,true);channel_config_set_ring(&c,false,10);channel_config_set_write_increment(&c,false);channel_config_set_dreq(&c,dma_get_timer_dreq(timer));channel_config_set_chain_to(&c,channels[i^1]);
 volatile uint32_t *cc=&pwm_hw->slice[slice].cc;
 dma_channel_configure(channels[i],&c,cc,buffers[i],B,false);dma_irqn_set_channel_enabled(1,channels[i],true);}
 irq_set_exclusive_handler(DMA_IRQ_1,irq_audio);irq_set_enabled(DMA_IRQ_1,true);dma_channel_start(channels[0]);initialized=true;
}
#if WEBDIAG
// Core 0 only requests. Core 1 owns the IRQ and performs the complete shutdown.
void c64_audio_stop_request(void) {
 __atomic_store_n(&stop_requested,true,__ATOMIC_RELEASE);
 __sev();
}
void c64_audio_diag_poll(void) {
 if(!initialized || !__atomic_load_n(&stop_requested,__ATOMIC_ACQUIRE) ||
    __atomic_load_n(&dma_stopped,__ATOMIC_ACQUIRE)) return;
 irq_set_enabled(DMA_IRQ_1,false);
 for(int i=0;i<2;i++) dma_irqn_set_channel_enabled(1,channels[i],false);
 dma_timer_set_fraction(audio_timer,0,1);
 // RP2350-E5: disable BOTH chained channels before aborting either one.
 for(int i=0;i<2;i++) hw_clear_bits(&dma_hw->ch[channels[i]].ctrl_trig,DMA_CH0_CTRL_TRIG_EN_BITS);
 for(int i=0;i<2;i++) {
  dma_channel_abort(channels[i]);
  dma_irqn_acknowledge_channel(1,channels[i]);
 }
 pwm_set_enabled(pwm_gpio_to_slice_num(BOARD_AUDIO_PWM_PIN),false);
 gpio_init(BOARD_AUDIO_PWM_PIN);
 gpio_put(BOARD_AUDIO_PWM_PIN,0);
 gpio_set_dir(BOARD_AUDIO_PWM_PIN,GPIO_OUT);
 __atomic_store_n(&dma_stopped,true,__ATOMIC_RELEASE);
}
#endif
void audio_set(int on,int percent){if(on>=0)enabled=on!=0;if(percent>=0)volume=percent>200?200:percent;}
void audio_get(bool *on,unsigned *percent){*on=enabled;*percent=volume;}
void audio_command(const char *args){unsigned n;if(sscanf(args,"vol %u",&n)==1)audio_set(-1,n);printf("AUDIO SID PWM 44100 Hz vol=%u on=%u\n",volume,enabled);}
void audio_test(const char *args){(void)args;tone_remaining=44100;printf("C64 speaker test: one second\n");}
void audio_set_ring(void *p){(void)p;}size_t audio_ring_size(void){return sizeof ring;}
void audio_sn_write(uint64_t t,uint8_t v){(void)t;(void)v;}void audio_frame(uint64_t t,bool mute){(void)t;(void)mute;}

extern uint32_t c64_sid_state_size(void);
extern bool c64_sid_state_in_sram(void);
void c64_audio_json(char *out,uint32_t size){
 snprintf(out,size,",\"c64_audio\":{\"ready\":%s,\"on\":%s,\"volume\":%u,\"samples\":%lu,\"blocks\":%lu,\"underruns\":%lu,\"sid_min\":%d,\"sid_max\":%d,\"pwm_min\":%u,\"pwm_max\":%u,\"sid_state_bytes\":%lu,\"sid_state_sram\":%s}",
 initialized?"true":"false",enabled?"true":"false",volume,(unsigned long)produced,(unsigned long)blocks,(unsigned long)underruns,sid_min,sid_max,pwm_min,pwm_max,(unsigned long)c64_sid_state_size(),c64_sid_state_in_sram()?"true":"false");
}

#if WEBDIAG
void c64_audio_diag_json(char *out,uint32_t size) {
 unsigned busy=0;
 if(initialized) for(int i=0;i<2;i++) if(dma_channel_is_busy(channels[i])) busy|=1u<<i;
 snprintf(out,size,",\"c64_audio_hw\":{\"stop_requested\":%s,\"stopped\":%s,\"busy_mask\":%u}",
 __atomic_load_n(&stop_requested,__ATOMIC_ACQUIRE)?"true":"false",
 __atomic_load_n(&dma_stopped,__ATOMIC_ACQUIRE)?"true":"false",busy);
}
#endif
