#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "spectrum.h"
#include "tape.h"
#include <strings.h>
static spectrum_tape_t tape;
static uint8_t *tape_store;
static uint32_t tape_capacity, autoload_us;
static unsigned autoload_step;
static const char *media_error;
#define ZX_EAR_INPUT(s) (tape.mounted ? tape.level : (((s)->last_fe_out & 24)!=0))
static uint8_t *external_memory;
static uint8_t address_space[65536];
#define hot_rom address_space
#define hot_ram (*(uint8_t (*)[3][16384])(address_space+16384))
// The standalone 48K machine has no paging: reads use one flat SRAM address.
#define ZX_MEM_RD(s,a) (address_space[(uint16_t)(a)])
#define ZX_MEM_WR(s,a,v) do {if((uint16_t)(a)>=0x4000)address_space[(uint16_t)(a)]=(v);} while(0)
#define ZX_INLINE_TICK static inline __attribute__((always_inline))
static void (*line_sink)(unsigned,const spectrum_line_t *);
#ifdef PICO_ON_DEVICE
static spectrum_line_t lines[1];
#define LINE_INDEX(y) 0
#else
static spectrum_line_t lines[256];
#define LINE_INDEX(y) (y)
#endif
void spectrum_set_line_sink(void (*sink)(unsigned,const spectrum_line_t *)){line_sink=sink;}
#define ZX_CAPTURE_SCANLINE(s,y) do { \
 spectrum_line_t *l=&lines[LINE_INDEX(y)];l->border=(s)->border_color;l->blink=((s)->blink_counter&16)!=0; \
 if((y)>=32 && (y)<224){unsigned yy=(y)-32; \
 unsigned off=((yy&192)<<5)|((yy&7)<<8)|((yy&56)<<2); \
 memcpy(l->bits,(s)->ram[(s)->display_ram_bank]+off,32); \
 memcpy(l->attrs,(s)->ram[(s)->display_ram_bank]+0x1800+(yy/8)*32,32); } \
 if(line_sink)line_sink(y,l); \
} while(0)
#if defined(PICO_ON_DEVICE) && defined(SPECTRUM_TICK_PROFILE)
#include "pico/time.h"
static uint32_t profile_ticks=509,profile_last,profile_samples,profile_us[4];
static bool profile_active;
static inline void profile_tick(unsigned stage){
 if(stage==0){profile_active=(--profile_ticks)==0;if(profile_active){profile_ticks=509;profile_last=time_us_32();profile_samples++;}}
 else if(profile_active){uint32_t t=time_us_32();profile_us[stage-1]+=t-profile_last;profile_last=t;}
}
#define ZX_PROFILE(stage) profile_tick(stage)
#endif
#define ZX_EXTERNAL_MEMORY(s) do { \
 (s)->rom[0]=hot_rom; (s)->rom[1]=external_memory+229440; (s)->fb=external_memory; (s)->junk=external_memory+213056; \
 for(int b=0;b<8;b++) (s)->ram[b]=b<3?hot_ram[b]:external_memory+131072+(b-3)*16384; \
} while(0)
#define mem_init zx_mem_init
#define CHIPS_IMPL
#include "chips/chips_common.h"
#define Z80_TICK_ATTR static __attribute__((always_inline)) inline
#define ZX_NO_DEBUG
#include "chips/z80.h"
#define ZX_48K_ONLY
#define ZX_AUDIO_COLD_SAMPLE
#define BEEPER_TICK_ATTR static inline __attribute__((always_inline))
#ifdef SPECTRUM_AUDIO_PROFILE
#ifdef PICO_ON_DEVICE
#include "pico/time.h"
#else
// Native validation supplies a deterministic timer; no hardware timer is needed.
extern uint32_t spectrum_test_time_us(void);
#define time_us_32 spectrum_test_time_us
#endif
static spectrum_audio_work_t audio_work[2];
static uint32_t audio_start[2], audio_until_sample[2] = {127, 127};
static bool audio_measuring[2];
static inline void audio_work_begin(unsigned kind) {
    spectrum_audio_work_t *p = &audio_work[kind];
    ++p->events;
    audio_measuring[kind] = (--audio_until_sample[kind] == 0);
    if (audio_measuring[kind]) {
        audio_until_sample[kind] = 127;
        ++p->samples;
        uint32_t t = time_us_32();
        uint32_t now = time_us_32();
        p->timer_baseline_us += now - t;
        audio_start[kind] = time_us_32();
    }
}
static inline void audio_work_end(unsigned kind) {
    if (audio_measuring[kind]) audio_work[kind].measured_us += time_us_32() - audio_start[kind];
}
#define BEEPER_HISTORY_BEGIN() audio_work_begin(0)
#define BEEPER_HISTORY_END() audio_work_end(0)
#endif
#define BEEPER_LAZY_HISTORY
#define BEEPER_BINARY_FIXED_GAIN
#include "chips/beeper.h"
#include "chips/ay38910.h"
#include "chips/mem.h"
#include "chips/kbd.h"
#include "chips/clk.h"
#include "systems/zx.h"
extern const uint8_t spectrum_rom[];
static zx_t zx;
static uint32_t total_frames;
static void audio_cb(const float *s,int n,void *user){
 (void)user;
#ifdef SPECTRUM_AUDIO_PROFILE
 audio_work_begin(1);
#endif
 spectrum_audio(s,n);
#ifdef SPECTRUM_AUDIO_PROFILE
 audio_work_end(1);
#endif
}
void spectrum_audio_work_get(spectrum_audio_work_t out[2]) {
#ifdef SPECTRUM_AUDIO_PROFILE
 memcpy(out, audio_work, sizeof audio_work);
#else
 memset(out, 0, 2*sizeof *out);
#endif
}
void spectrum_init(uint8_t *external){
 external_memory=external;memset(&tape,0,sizeof tape);autoload_us=0;media_error=NULL;
 memset(hot_ram,0,sizeof hot_ram);
 memset(external,0,131072+5*16384);
 zx_desc_t d={.type=ZX_TYPE_48K,.joystick_type=ZX_JOYSTICKTYPE_KEMPSTON,
  .audio={.callback={.func=audio_cb},.num_samples=128,.sample_rate=44100,.beeper_volume=0.5f},
  .roms={.zx48k={.ptr=(void*)spectrum_rom,.size=16384}}};
 zx_init(&zx,&d);total_frames=0;
}
static uint16_t word(const uint8_t *p){return p[0]|((uint16_t)p[1]<<8);}
bool spectrum_load_sna(const uint8_t *p,uint32_t n){
 // Only the 48K format. Validate everything before changing the running machine.
 if(!p || n!=49179 || p[25]>2 || p[26]>7)return false;
 uint16_t sp=word(p+23);
 if(sp<0x4000 || sp>0xfffe)return false;
 uint16_t pc=word(p+27+sp-0x4000);
 memset(&tape,0,sizeof tape);autoload_us=0;
 zx_reset(&zx);
 for(int b=0;b<3;b++)memcpy(zx.ram[b],p+27+b*16384,16384);
 z80_t *c=&zx.cpu;
 c->i=p[0];c->hl2=word(p+1);c->de2=word(p+3);c->bc2=word(p+5);c->af2=word(p+7);
 c->hl=word(p+9);c->de=word(p+11);c->bc=word(p+13);c->iy=word(p+15);c->ix=word(p+17);
 c->iff1=c->iff2=(p[19]&4)!=0;c->r=p[20];c->af=word(p+21);c->sp=(uint16_t)(sp+2);c->im=p[25];
 zx.border_color=p[26];zx.pins=z80_prefetch(c,pc);memset(zx.physical_keys,0,8);
 return true;
}
// Tape playback splits execution only at signal edges, never adding a per-tick
// callback to the normal CPU loop. Pulses are measured in emulated T-states.
static void autoload_tick(unsigned us){
 if(!autoload_us)return;
 if(us<autoload_us){autoload_us-=us;return;}
 autoload_us=100000;memset(zx.physical_keys,0,8);
 // OpenSE BASIC uses typed keywords, unlike the original single-key ROM.
 switch(autoload_step++){
 case 0:zx.physical_keys[6]=1<<1;break; // L
 case 2:zx.physical_keys[5]=1<<1;break; // O
 case 4:zx.physical_keys[1]=1;break; // A
 case 6:zx.physical_keys[1]=1<<2;break; // D
 case 8:zx.physical_keys[7]=1;break; // space
 case 10:case 12:zx.physical_keys[7]=1<<1;zx.physical_keys[5]=1;break; // Symbol Shift + P
 case 14:zx.physical_keys[6]=1;break; // Enter
 case 16:autoload_us=0;spectrum_tape_play(&tape);break;
 default:break;
 }
}
void spectrum_exec(unsigned us){
 uint8_t b=zx.blink_counter;autoload_tick(us);
 if(!tape.playing)zx_exec(&zx,us);
 else{
  uint32_t ticks=clk_us_to_ticks(zx.freq_hz,us);
  while(ticks){
   if(!tape.playing){zx_exec_ticks(&zx,ticks);break;}
   uint32_t n=ticks<tape.remaining?ticks:tape.remaining;
   zx_exec_ticks(&zx,n);ticks-=n;tape.remaining-=n;
   if(!tape.remaining)spectrum_tape_next(&tape);
  }
  kbd_update(&zx.kbd,us);
 }
 total_frames+=(uint8_t)(zx.blink_counter-b);
}
void spectrum_key(unsigned code,bool down){if(code<40){uint8_t mask=1u<<(code%5);if(down)zx.physical_keys[code/5]|=mask;else zx.physical_keys[code/5]&=~mask;}}
const uint8_t *spectrum_pixels(void){return zx.fb;}
uint32_t spectrum_frames(void){return total_frames;}
uint16_t spectrum_pc(void){return zx.cpu.pc;}

void spectrum_joystick(unsigned d,bool fire){zx_joystick(&zx,((d&1)?8:0)|((d&2)?4:0)|((d&4)?2:0)|((d&8)?1:0)|(fire?16:0));}

// Render only the captured immutable record; never read live emulated RAM.
void spectrum_render_record(const spectrum_line_t *l,unsigned y,uint8_t out[320]){
 memset(out,l->border,320);
 if(y<32||y>=224)return;
 for(unsigned x=0;x<32;x++){
  unsigned a=l->attrs[x],fg=(a&7)|((a&64)>>3),bg=((a>>3)&7)|((a&64)>>3);
  if((a&128)&&l->blink){unsigned t=fg;fg=bg;bg=t;}
  for(unsigned bit=0;bit<8;bit++)out[32+x*8+bit]=(l->bits[x]&(128>>bit))?fg:bg;
 }
}
void spectrum_profile_get(uint32_t out[5]){
#if defined(PICO_ON_DEVICE) && defined(SPECTRUM_TICK_PROFILE)
 out[0]=profile_samples;for(int i=0;i<4;i++)out[i+1]=profile_us[i];
#else
 memset(out,0,5*sizeof *out);
#endif
}

void spectrum_render_line(unsigned y,uint8_t out[320]){spectrum_render_record(&lines[LINE_INDEX(y)],y,out);}

void spectrum_media_storage(uint8_t *p,uint32_t n){tape_store=p;tape_capacity=n;}
const char *spectrum_media_error(void){return media_error?media_error:"Invalid Spectrum image";}
const char *spectrum_tape_state(void){return tape.error?"error":!tape.mounted?"none":autoload_us?"starting":tape.playing?"playing":tape.pos==tape.size&&!tape.remaining&&!tape.phase?"finished":"stopped";}
uint32_t spectrum_tape_block(void){return tape.block;}
uint32_t spectrum_tape_blocks(void){return tape.blocks;}
const char *spectrum_tape_error(void){return tape.error?tape.error:"";}
bool spectrum_tape_command(const char *cmd){
 if(!tape.mounted)return false;
 if(!strcmp(cmd,"stop")){tape.playing=false;autoload_us=0;memset(zx.physical_keys,0,8);}
 else if(!strcmp(cmd,"rewind")){spectrum_tape_rewind(&tape);autoload_us=0;memset(zx.physical_keys,0,8);}
 else if(!strcmp(cmd,"play")){autoload_us=0;memset(zx.physical_keys,0,8);spectrum_tape_play(&tape);}
 else if(!strcmp(cmd,"load")){
  spectrum_tape_rewind(&tape);zx_reset(&zx);memset(zx.physical_keys,0,8);
  autoload_step=0;autoload_us=1500000;
 }else return false;
 return true;
}

#include "snapshot.inc"
