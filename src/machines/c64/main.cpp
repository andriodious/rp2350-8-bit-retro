#include "startup_diag.h"
#if C64_FULL_1581
#include "drive_bridge.h"
#endif
#if NIMBUS_TEST
#include "controller.h"
#endif
#include "profile.h"
#include "t64.h"
#include "crt.h"
#include "pixel_convert.h"
#include <atomic>
#include <cstdio>
#include <cstring>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "pico/flash.h"
#include "hardware/clocks.h"
#include "hardware/vreg.h"
#include "hardware/structs/qmi.h"
#include "hardware/watchdog.h"
#include "hardware/structs/watchdog.h"
#include "hardware/structs/scb.h"
#include "board.h"
#include "machine_if.h"
#include "frontend.h"
#if BOARD_HAS_NET
#include "net_if.h"
#endif
#include "drive.h"
#include "psram_allocator.h"
extern "C" {
#include "sound.h"
extern const uint8_t c64_default_disc[];
extern const uint32_t c64_default_disc_size;
void c64_init(void); bool c64_run_frame(void); void c64_reset(void); void c64_nmi(void);
uint8_t *c64_exchange_framebuffer(void);
uint8_t *c64_get_framebuffer(void); uint8_t *c64_get_ram(void);
void c64_mount_disk(const uint8_t *,uint32_t,const char *);void c64_unmount_disk(void);
void c64_disk_set(uint8_t *,size_t);uint32_t c64_disk_writes(void);uint32_t c64_memory_used(void);
}
#if !WITH_SOUND
extern "C" void sid_add_sample(int16_t,int16_t){}
extern "C" int sid_get_buffer_fill(){return 0;}
#endif
#if C64_FULL_1581
static uint32_t drive_net_calls, drive_net_max_gap, drive_net_last;
static uint64_t drive_net_us;
extern "C" void retro_drive_network_poll(void) {
#if BOARD_HAS_NET
    const uint32_t now=time_us_32();
    // Bound radio servicing overhead while the full drive saturates core 0.
    if(drive_net_calls && (uint32_t)(now-drive_net_last)<5000u)return;
    if(drive_net_calls){const uint32_t gap=now-drive_net_last;if(gap>drive_net_max_gap)drive_net_max_gap=gap;}
    drive_net_last=now;
    // Only move packets. net_frame/USB/API commands may replace the emulated
    // machine or mounted media, so those must remain at the frame boundary.
    net_poll();
    drive_net_us+=(uint32_t)(time_us_32()-now);
    drive_net_calls++;
#endif
}
#endif
// Scratch 0..3 survive watchdog resets; SDK boot control uses 4..7.
static constexpr uint32_t DIAG_MAGIC=0x64360000;
static bool recovery;
static uint32_t previous_phase, previous_pc, previous_cfsr;
static volatile bool fault_seen;
static uint32_t trace_frames;
static volatile uint32_t trace_rendered;
static void phase(uint32_t n){if(!fault_seen)watchdog_hw->scratch[0]=DIAG_MAGIC|(n&255);}
extern "C" void c64_network_phase(uint32_t n){phase(n);}
extern "C" void c64_fault(uint32_t *frame){
    fault_seen=true;
    watchdog_hw->scratch[2]=frame[6];
    watchdog_hw->scratch[1]=scb_hw->cfsr;
    watchdog_hw->scratch[3]=scb_hw->bfar;
    watchdog_hw->scratch[0]|=get_core_num()<<8;
    // Let the armed watchdog expire, preserving its reset-cause marker.
    for(;;)__wfi();
}
extern "C" void __attribute__((naked)) isr_hardfault(){
    __asm volatile("tst lr, #4\n ite eq\n mrseq r0, msp\n mrsne r0, psp\n"
                   "b c64_fault\n");
}
extern "C" void c64_diagnostics_json(char *out,uint32_t size){
    snprintf(out,size,",\"c64\":{\"build\":\"r11\",\"recovery\":%s,\"phase\":%lu,\"pc\":\"%08lx\",\"cfsr\":\"%08lx\",\"uptime_ms\":%lu,\"live_phase\":%lu,\"frames\":%lu,\"rendered\":%lu}",
        recovery?"true":"false",(unsigned long)previous_phase,(unsigned long)previous_pc,(unsigned long)previous_cfsr,
        (unsigned long)to_ms_since_boot(get_absolute_time()),(unsigned long)(watchdog_hw->scratch[0]&511),
        (unsigned long)trace_frames,(unsigned long)trace_rendered);
#if C64_PROFILE
    size_t used = strlen(out);
    if (used < size) c64_profile::json(out + used, size - used);
#endif
}
static bool turbo, reset_pending, install_hold;
static uint8_t keys[8],reverse_keys[8];
static uint32_t boot_frames;
static t64_archive tape;
static uint32_t tape_generation;
static unsigned tape_selected;
static bool tape_pending, tape_run, boot_requested;
static const char *tape_state = "none";
static const char *typing;
static uint8_t joystick=255;
static uint8_t controller_joy[2]={255,255};
static bool pressed[256];
static void rebuild_keys(){
 memset(keys,255,8);memset(reverse_keys,255,8);joystick=255;
 for(unsigned c=0;c<256;c++)if(pressed[c]){
 unsigned code=c;
 if(c>=129 && c<=133){joystick&=~(1u<<(c-129));continue;}
 if(c>=64 && c<128){code-=64;keys[1]&=~128;reverse_keys[7]&=~2;}
 if(code>=64)continue;
 unsigned row=code/8,bit=code%8;keys[row]&=~(1u<<bit);reverse_keys[bit]&=~(1u<<row);
 }
}
static unsigned speed=100, load0, load1;
// One RGB565 row per VIC row; the LCD row table stretches 272 rows to 480.
static uint16_t pixels[272][482] __attribute__((aligned(4)));
static const uint16_t *rows[481];
static_assert(std::atomic<bool>::is_always_lock_free, "Frame hand-off must be lock-free");
static std::atomic<bool> frame_ready{false};
static const uint8_t *render_source;
static bool render_pending;
static volatile uint32_t conversion_us;
static const uint32_t palette[16]={0x000000,0xffffff,0x813338,0x75cec8,0x8e3c97,0x56ac4d,0x2e2c9b,0xedf171,0x8e5029,0x553800,0xc46c71,0x4a4a4a,0x7b7b7b,0xa9ff9f,0x706deb,0xb2b2b2};
static uint16_t pin_palette[16];
static void convert_frame(){
    const uint8_t *src=render_source;
    for(int y=0;y<272;y++)for(int group=0;group<96;group++){
        const uint8_t *in=src+y*384+group*4;uint16_t *out=pixels[y]+group*5;
#if C64_PACKED_PIXELS
        c64_convert_four<true>(in,out,pin_palette);
#else
        out[0]=out[1]=pin_palette[in[0]&15];out[2]=pin_palette[in[1]&15];
        out[3]=pin_palette[in[2]&15];out[4]=pin_palette[in[3]&15];
#endif
    }
}
static void core1(){
    flash_safe_execute_core_init();
#if WITH_SOUND
    audio_init();
#endif
    for(;;){
        if(!frame_ready.load(std::memory_order_acquire)){__wfe();continue;}
        __dmb();uint32_t t=time_us_32();convert_frame();trace_rendered++;conversion_us=time_us_32()-t;
#if WITH_AMBIENT
        if(board_leds_due())board_leds_update();
#endif
        frame_ready.store(false,std::memory_order_release);__sev();
    }
}
// App installation is shared with BBC modes. Keep the last C64 picture while
// paused; the BBC splash renderer cannot draw into this RGB565 framebuffer.
extern "C" void machine_install_screen(const char *, int permille){
    install_hold = permille >= 0;
}
extern "C" const char *machine_name(){return "c64";}
extern "C" void MACHINE_KEY_IMPL(int code,bool down){
    if(recovery)return;
    // A real key takes over from the startup typing, including during its delay.
    if(down){typing=nullptr;boot_frames=0;if(tape_pending)tape_state="cancelled";tape_pending=false;}
    if(code==134){if(down)c64_nmi();return;}
    if(code<0 || code>=256)return;
    pressed[code]=down;rebuild_keys();
}
extern "C" void c64_keyboard_poll(uint8_t *k,uint8_t *r,uint8_t *j){memcpy(k,keys,8);memcpy(r,reverse_keys,8);*j=controller_joy[0];}
extern "C" uint8_t input_get_joystick2(){return joystick & controller_joy[1];}
extern "C" void machine_key_sdl(uint32_t s,bool down){
    static const uint8_t letters[]={10,28,20,18,14,21,26,29,33,34,37,42,36,39,38,41,62,17,13,22,30,31,9,23,25,12};
    static const uint8_t digits[]={56,59,8,11,16,19,24,27,32,35};
    if(s>=4 && s<=29){machine_key(letters[s-4],down);return;}
    if(s>=30 && s<=39){machine_key(digits[s-30],down);return;}
    int c=-1;switch(s){case 40:c=1;break;case 41:c=63;break;case 42:c=0;break;case 44:c=60;break;
    case 45:c=43;break;case 46:c=53;break;case 51:c=50;break;case 54:c=47;break;case 55:c=44;break;case 56:c=55;break;
    case 58:c=4;break;case 59:c=68;break;case 60:c=5;break;case 61:c=69;break;case 62:c=6;break;case 63:c=70;break;case 64:c=3;break;case 65:c=67;break;
    case 74:c=51;break;case 79:c=2;break;case 80:c=66;break;case 81:c=7;break;case 82:c=71;break;
    case 224:case 228:c=58;break;case 225:case 229:c=15;break;case 226:case 230:c=61;break;}
    if(c>=0)machine_key(c,down);
}
extern "C" void machine_break(){if(recovery){watchdog_reboot(0,0,10);return;}reset_pending=true;
    if(!boot_requested){boot_frames=0;typing=nullptr;tape_pending=false;if(tape.count)tape_state="ready";}
    boot_requested=false;
}
extern "C" void machine_boot_shift(){
    boot_requested=true;boot_frames=c64_crt_active()?0:100;
    if(tape.count){tape_pending=true;tape_state="loading";}
}
extern "C" bool machine_turbo_get(){return turbo;}
extern "C" void machine_turbo_set(bool on){turbo=on;}
extern "C" bool machine_overlay_has(){return false;}
extern "C" bool machine_overlay_get(){return false;}
extern "C" void machine_overlay_set(bool){}
extern "C" void machine_perf_get(unsigned *a,unsigned *b,unsigned *s){*a=load0;*b=load1;*s=speed;}
extern "C" uint32_t machine_psram_used(){return c64_memory_used();}
extern "C" unsigned machine_leds(){return 0;}
extern "C" void machine_print_stats(){printf("STATS machine=c64 core0=%u core1=%u speed=%u%%\n",load0,load1,speed);}
extern "C" bool machine_usb_command(const char *cmd){
#if C64_PROFILE
if(!strcmp(cmd,"c64prof")){c64_profile::print();return true;}
#endif
if(!strcmp(cmd,"restore")){c64_nmi();return true;}return false;}
extern "C" void machine_drive_load(const uint8_t *image,uint32_t n,bool,bool boot){
    if(recovery)return;
    c64_crt_detach();
    tape={};tape_generation++;tape_pending=false;tape_state="none";
    c64_unmount_disk();typing=nullptr;boot_frames=0;
    uint8_t *dst=board_psram()+1024*1024;
    if(image && n){memmove(dst,image,n);c64_disk_set(dst,n);c64_mount_disk(dst,n,"presto.d64");}
    else c64_disk_set(nullptr,0);
    if(boot){reset_pending=true;boot_frames=image?100:0;
#if C64_FULL_1581
        if(retro_drive_active())boot_frames=250; // Original KERNAL performs its RAM test.
#endif
    }
}
// Retain the archive in the mounted-media PSRAM region, never in upload staging.
extern "C" unsigned c64_tape_count(){return tape.count;}
extern "C" uint32_t c64_tape_generation(){return tape_generation;}
extern "C" bool c64_tape_entry(unsigned index,t64_entry *out){return t64_entry_get(&tape,index,out);}
extern "C" bool c64_tape_select(unsigned index,bool run){
    t64_entry e;
    if(recovery || !t64_entry_get(&tape,index,&e) || (run && !e.runnable))return false;
    tape_selected=index;tape_run=run;tape_pending=true;tape_state="loading";
    typing=nullptr;reset_pending=true;boot_frames=100;return true;
}
extern "C" bool c64_tape_insert(const uint8_t *image,uint32_t n){
    t64_archive candidate;
    if(recovery || n>DRIVE_IMAGE_MAX || t64_open(&candidate,image,n))return false;
    c64_crt_detach();
    c64_unmount_disk();c64_disk_set(nullptr,0);typing=nullptr;boot_frames=0;tape_pending=false;
    uint8_t *dst=board_psram()+1024*1024;
    memmove(dst,image,n);candidate.data=dst;tape=candidate;tape_generation++;
    tape_selected=0;tape_run=false;tape_state="ready";reset_pending=true;
    if(tape.count==1){t64_entry e;t64_entry_get(&tape,0,&e);c64_tape_select(0,e.runnable);}
    return true;
}
extern "C" bool c64_cartridge_insert(const uint8_t *image,uint32_t n){
    if(recovery || c64_crt_prepare(image,n))return false;
    c64_unmount_disk();c64_disk_set(nullptr,0);
    tape={};tape_generation++;tape_pending=false;tape_state="none";
    typing=nullptr;boot_frames=0;reset_pending=true;return true;
}
extern "C" void c64_cartridge_eject(){
    if(recovery || !c64_crt_active())return;
    drive0_unload();
}
extern "C" void c64_tape_status(char *out,uint32_t size){
    snprintf(out,size,",\"tape\":{\"generation\":%lu,\"count\":%u,\"selected\":%u,\"state\":\"%s\"}",
        (unsigned long)tape_generation,tape.count,tape_selected,tape_state);
    size_t used=strlen(out);if(used<size)snprintf(out+used,size-used,",\"cartridge\":{\"active\":%s}",c64_crt_active()?"true":"false");
#if C64_FULL_1581
    used=strlen(out);if(used<size)snprintf(out+used,size-used,",\"c64_net\":{\"calls\":%lu,\"poll_us\":%llu,\"max_gap_us\":%lu}",(unsigned long)drive_net_calls,(unsigned long long)drive_net_us,(unsigned long)drive_net_max_gap);
    used=strlen(out);if(used<size)snprintf(out+used,size-used,",\"drive1581\":{\"active\":%s,\"read_only\":true,\"pc\":\"%04x\",\"run_us\":%llu,\"run_calls\":%llu,\"instructions\":%llu,\"empty_calls\":%llu,\"empty_us\":%llu,\"idle_loops\":%llu,\"idle_tail_instructions\":%llu}",retro_drive_active()?"true":"false",retro_drive_active()?retro_drive_pc():0,(unsigned long long)retro_drive_run_us(),(unsigned long long)retro_drive_run_calls(),(unsigned long long)retro_drive_instructions(),(unsigned long long)retro_drive_empty_calls(),(unsigned long long)retro_drive_empty_us(),(unsigned long long)retro_drive_idle_loops(),(unsigned long long)retro_drive_idle_tail_instructions());
#endif
}
extern "C" uint32_t machine_drive_writes(){return c64_disk_writes();}
extern "C" bool machine_drive_sector(int,int,uint8_t *){return false;}
extern "C" void machine_drive_clean(){}
static void __no_inline_not_in_flash_func(slow_flash)(){
    hw_write_masked(&qmi_hw->m[0].timing,(3u<<QMI_M0_TIMING_CLKDIV_LSB)|(2u<<QMI_M0_TIMING_RXDELAY_LSB),QMI_M0_TIMING_CLKDIV_BITS|QMI_M0_TIMING_RXDELAY_BITS);
}
int main(){
    startup_diag_mark(0x201);
    slow_flash();startup_diag_mark(0x202);
    vreg_set_voltage(VREG_VOLTAGE_1_25);set_sys_clock_khz(250000,true);
    startup_diag_mark(0x203);
    recovery=watchdog_enable_caused_reboot() && (watchdog_hw->scratch[0]&0xffff0000u)==DIAG_MAGIC;
    previous_phase=watchdog_hw->scratch[0]&511;previous_pc=watchdog_hw->scratch[2];previous_cfsr=watchdog_hw->scratch[1];
    watchdog_disable();
    stdio_init_all();startup_diag_mark(0x204);board_init();board_psram_init();c64_memory_init();
    if(recovery){
        printf("C64 RECOVERY phase=%lu pc=%08lx cfsr=%08lx\n",(unsigned long)previous_phase,(unsigned long)previous_pc,(unsigned long)previous_cfsr);
        frontend_init();
        for(;;){frontend_frame();sleep_ms(20);}
    }
    watchdog_hw->scratch[0]=DIAG_MAGIC;watchdog_hw->scratch[1]=0;watchdog_hw->scratch[2]=0;watchdog_hw->scratch[3]=0;
    phase(1);watchdog_enable(8000,true);
    memset(keys,255,8);memset(reverse_keys,255,8);
    for(int i=0;i<16;i++){
        uint32_t p=palette[i],v=((p>>8)&0xf800)|((p>>5)&0x07e0)|((p>>3)&0x1f),rev;
        __asm("rbit %0, %1":"=r"(rev):"r"(v<<16));pin_palette[i]=(uint16_t)rev;
    }
#if WITH_AMBIENT
    board_leds_init();
#endif
    for(int y=0;y<480;y++)rows[y]=pixels[y*272/480];
    rows[480]=nullptr;
    board_display_init(rows);phase(2);c64_init();phase(3);drive0_init(c64_default_disc,c64_default_disc_size,board_psram());boot_frames=100;frontend_init();
    multicore_launch_core1(core1);watchdog_enable(8000,true);
    uint64_t due=time_us_64(),stat=due,busy=0;unsigned frames=0;
    for(;;){
        if(install_hold){
#if C64_PROFILE
            c64_profile::reset();
#endif
            frontend_frame();watchdog_update();sleep_ms(1);
            due=time_us_64();stat=due;busy=0;frames=0;
            continue;
        }
        if(reset_pending){c64_reset();
#if NIMBUS_TEST
    controller_reset();
#endif
reset_pending=false;memset(pressed,0,sizeof pressed);rebuild_keys();}
#if C64_PROFILE
        c64_profile::begin(time_us_64(),turbo);
#endif
#if C64_FULL_1581
        if(!retro_drive_enabled){drive_net_calls=0;drive_net_us=0;drive_net_max_gap=0;drive_net_last=0;}
#endif
        phase(4);uint64_t t=time_us_64();
        C64_PROFILE_CALL(EMULATION, c64_run_frame());
        trace_frames++;busy+=time_us_64()-t;
        phase(5);
#if WITH_AMBIENT
        board_leds_frame(0);
#endif
        // Turbo skips alternate display conversions, giving emulation more time.
        bool submitted = !turbo || (trace_frames&1)==0;
        bool converted = false;
#if C64_RENDER_OVERLAP
        // Emulation has used its own writable buffer while core 1 reads the
        // previous completed frame. Wait only before reusing that reader's buffer.
        C64_PROFILE_CALL(WAIT, while(frame_ready.load(std::memory_order_acquire)){frontend_poll();sleep_us(50);});
        converted=render_pending;
#if C64_PROFILE
        if(converted)c64_profile::add(c64_profile::CONVERSION,conversion_us);
#endif
        render_pending=false;
        if(submitted){
            render_source=c64_exchange_framebuffer();
            render_pending=true;
            frame_ready.store(true,std::memory_order_release);__sev();
        }
#else
        if(submitted){
            render_source=c64_get_framebuffer();
            frame_ready.store(true,std::memory_order_release);__sev();
        }
        C64_PROFILE_CALL(WAIT, while(frame_ready.load(std::memory_order_acquire)){frontend_poll();sleep_us(50);});
        converted=submitted;
#if C64_PROFILE
        if(converted)c64_profile::add(c64_profile::CONVERSION,conversion_us);
#endif
#endif
        phase(6);C64_PROFILE_CALL(FRONTEND, frontend_frame());watchdog_update();
        if(boot_frames && !--boot_frames){
            if(tape_pending){
                tape_pending=false;
                if(t64_load(&tape,tape_selected,c64_get_ram())){
                    tape_state="loaded";if(tape_run)typing="RUN\r";
                }else tape_state="error";
            }else if(!tape.count && !c64_crt_active())typing="LOAD\"*\",8,1\rRUN\r";
        }
        if(typing && *typing){uint8_t *ram=c64_get_ram();if(ram[198]<10){ram[631+ram[198]]= *typing++;ram[198]++;}}
        phase(7);frames++;due+=20000;
        C64_PROFILE_CALL(PACING,
            if(!turbo){while(time_us_64()<due){frontend_poll();sleep_us(100);}}
            else due=time_us_64(););
#if C64_PROFILE
        if(c64_profile::finish(time_us_64(),converted))c64_profile::print();
#endif
        if(time_us_64()-due>100000)due=time_us_64();
        uint64_t now=time_us_64();if(now-stat>=5000000){uint64_t dt=now-stat;load0=busy*100/dt;load1=conversion_us/200;speed=frames*2000000ull/dt;machine_print_stats();stat=now;busy=0;frames=0;}
    }
}

#if NIMBUS_TEST
extern "C" void machine_controller(unsigned port,uint16_t x,uint16_t y,unsigned directions,bool fire) {
    (void)x;(void)y;if(port<1||port>2)return;
    controller_joy[port-1]=(uint8_t)~((directions&15)|(fire?16:0));
}
#endif
