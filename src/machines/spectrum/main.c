#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "pico/flash.h"
#include "hardware/clocks.h"
#include "hardware/vreg.h"
#include "hardware/structs/qmi.h"
#include "hardware/watchdog.h"
#include "board.h"
#include "frontend.h"
#include "machine_if.h"
#include "drive.h"
#include "sound.h"
#include "spectrum.h"
#include "hang_diag.h"
#include "line_queue.h"
extern const uint8_t spectrum_elite[];
extern const unsigned spectrum_elite_size;
void spectrum_add_sample(int16_t,int16_t);
static uint16_t pixels[256][482] __attribute__((aligned(4))),black[482];
static const uint16_t *rows[481];
static uint16_t palette[16];
static spectrum_queue_t queue;
static unsigned submitted,completed,queue_stalls,queue_wait_us;
static volatile bool ambient_pending;
static volatile uint32_t render_us;
static bool turbo,hold;
#if BOOT_STATUS_TRIAL
static bool status_drawing;
uint32_t *machine_status_framebuffer(bool active){
 status_drawing=active;
 while(__atomic_load_n(&completed,__ATOMIC_ACQUIRE)!=submitted){sleep_us(30);}
 return (uint32_t *)pixels;
}
#endif
static unsigned load0,load1,speed=100;
static uint32_t prof_window,prof_frames,prof_exec,prof_poll,prof_wait,prof_front,prof_draw;
void spectrum_status_json(char *dst,uint32_t size){
 uint32_t p[5];spectrum_profile_get(p);
 snprintf(dst,size,",\"spectrum_profile\":{\"version\":3,\"tick_sampling\":false,\"window_us\":%lu,\"frames\":%lu,\"exec_us\":%lu,\"poll_us\":%lu,\"render_wait_us\":%lu,\"frontend_us\":%lu,\"render_us\":%lu,\"tick_samples\":%lu,\"sample_cpu_us\":%lu,\"sample_video_us\":%lu,\"sample_bus_us\":%lu,\"sample_audio_us\":%lu}",
 (unsigned long)prof_window,(unsigned long)prof_frames,(unsigned long)prof_exec,(unsigned long)prof_poll,(unsigned long)prof_wait,(unsigned long)prof_front,(unsigned long)prof_draw,
 (unsigned long)p[0],(unsigned long)p[1],(unsigned long)p[2],(unsigned long)p[3],(unsigned long)p[4]);
 size_t n=strlen(dst);if(n<size)snprintf(dst+n,size-n,",\"spectrum_pipeline\":{\"slots\":8,\"bytes\":%u,\"submitted\":%u,\"completed\":%u,\"stalls\":%u,\"wait_us\":%u}",(unsigned)sizeof queue,submitted,__atomic_load_n(&completed,__ATOMIC_ACQUIRE),queue_stalls,queue_wait_us);
#ifdef SPECTRUM_AUDIO_PROFILE
 spectrum_audio_work_t work[2];spectrum_audio_work_get(work);
 n=strlen(dst);
 if(n<size)snprintf(dst+n,size-n,",\"spectrum_audio_profile\":{\"version\":1,\"stride\":127,\"cumulative\":true,\"history\":{\"events\":%lu,\"samples\":%lu,\"measured_us\":%lu,\"timer_baseline_us\":%lu},\"output\":{\"events\":%lu,\"samples\":%lu,\"measured_us\":%lu,\"timer_baseline_us\":%lu}}",
 (unsigned long)work[0].events,(unsigned long)work[0].samples,(unsigned long)work[0].measured_us,(unsigned long)work[0].timer_baseline_us,
 (unsigned long)work[1].events,(unsigned long)work[1].samples,(unsigned long)work[1].measured_us,(unsigned long)work[1].timer_baseline_us);
#endif
 n=strlen(dst);if(n<size)snprintf(dst+n,size-n,",\"spectrum_tape\":{\"state\":\"%s\",\"block\":%lu,\"blocks\":%lu,\"error\":\"%s\"}",spectrum_tape_state(),(unsigned long)spectrum_tape_block(),(unsigned long)spectrum_tape_blocks(),spectrum_tape_error());
 n=strlen(dst);if(n<size)spectrum_diag_status(dst+n,size-n);
}
static void submit_line(unsigned y,const spectrum_line_t *line){
#if BOOT_STATUS_TRIAL
 if(status_drawing)return;
#endif
 if(!spectrum_queue_push(&queue,y,line)){
  uint32_t start=time_us_32();
  do {queue_stalls++;__sev();frontend_poll();sleep_us(1);}while(!spectrum_queue_push(&queue,y,line));
  queue_wait_us+=time_us_32()-start;
 }
 submitted++;__sev();
}
static void convert_job(const spectrum_job_t *job){
 uint8_t line[320];spectrum_render_record(&job->line,job->y,line);
 for(int x=0;x<320;x+=2){uint16_t *d=pixels[job->y]+x*3/2;
 d[0]=d[1]=palette[line[x]&15];d[2]=palette[line[x+1]&15];}
}
static void core1(void){
 flash_safe_execute_core_init();audio_init();
 for(;;){
  spectrum_job_t job;
  if(spectrum_queue_pop(&queue,&job)){
   uint32_t t=time_us_32();convert_job(&job);
   __atomic_fetch_add(&render_us,time_us_32()-t,__ATOMIC_RELAXED);
   __atomic_fetch_add(&completed,1,__ATOMIC_RELEASE);__sev();continue;
  }
  if(__atomic_exchange_n(&ambient_pending,false,__ATOMIC_ACQUIRE)){
#if WITH_AMBIENT
   if(board_leds_due())board_leds_update();
#endif
  }
  __wfe();
 }
}
void spectrum_audio(const float *p,int n){for(int i=0;i<n;i++){int s=(int)(p[i]*24000.0f);if(s>32767)s=32767;if(s< -32768)s=-32768;spectrum_add_sample(s,s);}}
const char *machine_name(void){return "spectrum";}
void MACHINE_KEY_IMPL(int code,bool down){if(code>=0)spectrum_key((unsigned)code,down);}
void machine_key_sdl(uint32_t code,bool down){
 static const uint8_t letters[26]={5,39,3,7,12,8,9,34,27,33,32,31,37,38,26,25,10,13,6,14,28,4,11,2,29,1};
 if(code>=4&&code<=29)machine_key(letters[code-4],down);
 else if(code>=30&&code<=39){static const uint8_t nums[]={15,16,17,18,19,24,23,22,21,20};machine_key(nums[code-30],down);}
 else if(code==40)machine_key(30,down);else if(code==44)machine_key(35,down);
 else if(code==42||code==76||code==79||code==80||code==81||code==82||code==41){
  machine_key(0,down);
  machine_key(code==79?22:code==80?19:code==81?24:code==82?23:code==41?35:20,down);
 }
 else if(code==45||code==46||code==51||code==52||code==54||code==55||code==56){
  machine_key(36,down);
  machine_key(code==45?33:code==46?31:code==51?26:code==52?23:code==54?38:code==55?37:4,down);
 }
 else if(code==225||code==229)machine_key(0,down);else if(code==224||code==228)machine_key(36,down);
}
void machine_controller(unsigned port,uint16_t x,uint16_t y,unsigned directions,bool fire){(void)port;(void)x;(void)y;spectrum_joystick(directions,fire);}
void machine_break(void){spectrum_reset();}
void machine_boot_shift(void){}
bool machine_turbo_get(void){return turbo;}void machine_turbo_set(bool b){turbo=b;}
bool machine_overlay_has(void){return false;}bool machine_overlay_get(void){return false;}void machine_overlay_set(bool b){(void)b;}
void machine_perf_get(unsigned *a,unsigned *b,unsigned *c){*a=load0;*b=load1;*c=speed;}
uint32_t machine_psram_used(void){return 2097152+DRIVE_IMAGE_MAX;}
void machine_print_stats(void){printf("STATS machine=spectrum core0=%u core1=%u speed=%u%% frames=%lu pc=%04x\n",load0,load1,speed,(unsigned long)spectrum_frames(),spectrum_pc());}
unsigned machine_leds(void){return 0;}
void machine_drive_load(const uint8_t *p,uint32_t n,bool d,bool b){(void)d;(void)b;if(p)spectrum_load_sna(p,n);}
uint32_t machine_drive_writes(void){return 0;}bool machine_drive_sector(int t,int s,uint8_t *o){(void)t;(void)s;(void)o;return false;}void machine_drive_clean(void){}
void machine_install_screen(const char *what,int p){(void)what;hold=p>=0;}
bool machine_usb_command(const char *cmd){(void)cmd;return false;}
static void __no_inline_not_in_flash_func(slow_flash)(void){hw_write_masked(&qmi_hw->m[0].timing,(3u<<QMI_M0_TIMING_CLKDIV_LSB)|(2u<<QMI_M0_TIMING_RXDELAY_LSB),QMI_M0_TIMING_CLKDIV_BITS|QMI_M0_TIMING_RXDELAY_BITS);}
int main(void){
 slow_flash();vreg_set_voltage(VREG_VOLTAGE_1_25);set_sys_clock_khz(250000,true);
 watchdog_disable();spectrum_diag_init();stdio_init_all();board_init();
 if(board_psram_init()<2097152+DRIVE_IMAGE_MAX+BOARD_STATUS_BYTES)panic("Spectrum needs PSRAM");
 for(int i=0;i<16;i++){
  unsigned v=(i&8)?255:205,r=(i&2)?v:0,g=(i&4)?v:0,b=(i&1)?v:0;
  uint32_t rgb=((r>>3)<<11)|((g>>2)<<5)|(b>>3),rev;
  __asm("rbit %0, %1":"=r"(rev):"r"(rgb<<16));palette[i]=(uint16_t)rev;
 }
#if WITH_AMBIENT
 board_leds_init();
#endif
 for(int y=0;y<480;y++)rows[y]=(y<48||y>=432)?black:pixels[(y-48)*2/3];
 rows[480]=NULL;board_display_init(rows);
 spectrum_init(board_psram());spectrum_media_storage(board_psram()+2097152,DRIVE_IMAGE_MAX);drive0_init(spectrum_elite,spectrum_elite_size,board_psram()+262144);
 multicore_launch_core1(core1);spectrum_set_line_sink(submit_line);frontend_init();
 uint64_t due=time_us_64();uint32_t window=time_us_32(),busy=0,draw=0,frames=0,exec=0,poll=0,wait=0,front=0;
 for(;;){
  uint32_t start=time_us_32();
  if(!hold && !spectrum_diag_recovery()){for(int i=0;i<20;i++){uint32_t t=time_us_32();spectrum_diag_begin();spectrum_exec(i==19?968:1000);spectrum_diag_end();exec+=time_us_32()-t;t=time_us_32();frontend_poll();poll+=time_us_32()-t;}}

  uint32_t mark=time_us_32();
  while(__atomic_load_n(&completed,__ATOMIC_ACQUIRE)!=submitted){frontend_poll();sleep_us(30);}
  wait+=time_us_32()-mark;draw+=__atomic_exchange_n(&render_us,0,__ATOMIC_RELAXED);mark=time_us_32();frontend_frame();front+=time_us_32()-mark;
#if WITH_AMBIENT
  board_leds_frame(0);
  __atomic_store_n(&ambient_pending,true,__ATOMIC_RELEASE);__sev();
#endif
  busy+=time_us_32()-start;frames++;due+=19968;
  if(!turbo){while(time_us_64()<due){frontend_poll();sleep_us(100);}}
  if(time_us_64()>due+19968)due=time_us_64();
  uint32_t elapsed=time_us_32()-window;
  if(elapsed>=1000000){prof_window=elapsed;prof_frames=frames;prof_exec=exec;prof_poll=poll;prof_wait=wait;prof_front=front;prof_draw=draw;exec=poll=wait=front=0;load0=(uint64_t)busy*100/elapsed;load1=(uint64_t)draw*100/elapsed;speed=(uint64_t)frames*19968*100/elapsed;busy=draw=frames=0;window=time_us_32();}
 }
}
