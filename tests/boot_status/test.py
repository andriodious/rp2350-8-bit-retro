#!/usr/bin/env python3
"""Run the real status renderer/toggle code with mocked board IO, under sanitizers."""
from pathlib import Path
import subprocess, tempfile, os
root = Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory() as tmp:
    d=Path(tmp)
    source=(root/'src/common/boot_banner.c').read_text()
    # Host replacement for the one ARM-only pixel bit reversal instruction.
    source=source.replace('__asm("rbit %0, %1" : "=r"(pins) : "r"(rgb << 16));',
                          'pins = 0; for (unsigned bit=0; bit<16; bit++) pins |= ((rgb >> bit) & 1u) << (15-bit);')
    (d/'subject.c').write_text(source)
    for name in ['boot_button.h','boot_logo.h','boot_setup.inc','boot_setup_keys.h','boot_touch_keys.h','boot_touch_keyboard.inc','c64_runtime_diag.h']:
        (d/name).write_text((root/'src/common'/name).read_text())
    for name in ['system.h','api/api.h','boot_banner.h','board.h','drive.h','machine_if.h','net_if.h','pico/stdlib.h','hardware/watchdog.h','hardware/sync.h','firmware_identity.h','sound.h','controller.h','presto_bluetooth.h']:
        p=d/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_text('#include "mock.h"\n')
    (d/'mock.h').write_text(r'''
#pragma once
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define BOOT_STATUS_TRIAL 1
#define PRESTO_BEEBJIT_WIRE MACHINE_BEEB
#define RETRO_BUILD_ID "reference/four-machine-touch-menus-20261122-dirty"
#define WITH_SOUND 1
#define BOARD_STATUS_BYTES (512u*1024u)
#define BOARD_BOOT_BUTTON_PIN 46
#define BOARD_DISPLAY_HEIGHT 480
#if MACHINE_C64 || MACHINE_SPECTRUM
#define BOARD_DISPLAY_ROW_WORDS 241
#else
#define BOARD_DISPLAY_ROW_WORDS 61
#endif
#define BOARD_DISPLAY_ROW_END 8u
#define DRIVE_IMAGE_MAX (1024u*1024u)
#define BOARD_HAS_NET 0
#define NIMBUS_TEST 1
#define BT_KEYBOARD_CLASSIC 1
#define RETRO_IMAGE_ID "trial"
#define GPIO_IN 0
static uint8_t ram[8u*1024u*1024u];
static const uint16_t *emulator[481];
static const uint16_t *const *display=emulator;
static uint32_t now_ms, frames=100;
static bool down,muted,render_held,joystick_online,joystick_fire,captured;
static bool keyboard_online,keyboard_fire;
static unsigned keyboard_directions;
static unsigned joystick_directions,sd_count,sd_cursor;
static const char *sd_names[240];
static bool load_ok=true;
static bool touching;static unsigned touch_x,touch_y;
static bool board_touch(unsigned *x,unsigned *y){*x=touch_x;*y=touch_y;return touching;}
static void audio_get(bool *on,unsigned *vol){*on=true;*vol=20;}
static void board_leds_get(bool *on,unsigned *bright,bool *locks){*on=true;*bright=40;*locks=false;}
static bool launched=true,switch_ok=true;
static char switched[16],last_command[16],last_args[32];
typedef struct {const char *args,*from;bool json;int code;} api_req_t;
static bool system_launched(void){return launched;}
static bool api_call(const char *name,api_req_t *r){if(!strcmp(name,"keys"))assert(!captured&&!render_held&&!muted);snprintf(last_command,sizeof last_command,"%s",name);snprintf(last_args,sizeof last_args,"%s",r->args);snprintf(switched,sizeof switched,"%s",r->args);r->code=switch_ok?200:500;return true;}

static char loaded[256];
static bool controller_menu_state(unsigned *d,bool *f){*d=joystick_online?joystick_directions:0;*f=joystick_online&&joystick_fire;return joystick_online;}
static void controller_menu_capture(bool c){captured=c;}
static const char *presto_bluetooth_boot_status(void){return "CONNECTED";}
static void controller_keyboard_menu_state(unsigned *d,bool *f){*d=keyboard_directions;*f=keyboard_fire;}
static const char *presto_keyboard_boot_status(void){return keyboard_online?"CONNECTED":"PAIRING - SEE WEB";}
typedef struct {char address[18],pin[7];bool remembered,scanning,capturing,connected,connecting,reconnect;unsigned devices,captured_usage;} bluetooth_setup_state_t;
typedef struct {char address[18],name[40];} bluetooth_setup_device_t;
static bluetooth_setup_state_t bt_state;
static uint8_t bt_map[64];static int32_t bt_config[19]={1,2,0,15,0,0,71,7,66,2,-2,-1,-1,-1,-1,-1,-1,-1,-1};
static char bt_command[160];static int bt_result=200;
static void presto_bluetooth_setup_state(bool keyboard,bluetooth_setup_state_t *s){(void)keyboard;*s=bt_state;}
static bool presto_bluetooth_setup_device(bool keyboard,unsigned index,bluetooth_setup_device_t *d){(void)keyboard;if(index>=bt_state.devices)return false;snprintf(d->address,sizeof d->address,"11:22:33:44:55:%02u",index);snprintf(d->name,sizeof d->name,"DEVICE %u",index);return true;}
static int presto_bluetooth_command(const char *s){snprintf(bt_command,sizeof bt_command,"%s",s);if(!strcmp(s,"keyboard capture"))bt_state.capturing=true;if(!strcmp(s,"keyboard capture-cancel")){bt_state.capturing=false;bt_state.captured_usage=0;}return bt_result;}
static void controller_get_config(int32_t *c){memcpy(c,bt_config,sizeof bt_config);}
static void controller_keyboard_map_get(uint8_t *m){memcpy(m,bt_map,64);}
static void presto_bluetooth_boot_poll(void){}
static void *board_sd_discs_begin(void){sd_cursor=0;return sd_count?&sd_cursor:NULL;}
static bool board_sd_discs_next(void *it,char *name,uint32_t cap,uint32_t *size){
 (void)it;if(sd_cursor==sd_count)return false;
 snprintf(name,cap,"%s",sd_names[sd_cursor++]);*size=100;return true;
}
static void board_sd_discs_end(void *it){(void)it;}
static bool board_sd_load_disc(const char *n){snprintf(loaded,sizeof loaded,"%s",n);return load_ok;}
static uint32_t sram_pixels[272*241];
static void audio_status_mute(bool mute){muted=mute;}
static uint32_t *machine_status_framebuffer(bool active){render_held=active;return sram_pixels;}
static void gpio_init(unsigned p){(void)p;}
static void gpio_set_dir(unsigned p,unsigned v){(void)p;(void)v;}
static void gpio_pull_up(unsigned p){(void)p;}
static bool gpio_get(unsigned p){(void)p;return !down;}
static uint32_t get_absolute_time(void){return now_ms;}
static uint32_t to_ms_since_boot(uint32_t t){return t;}
static uint64_t time_us_64(void){return (uint64_t)now_ms*1000;}
static void sleep_ms(unsigned n){now_ms+=n;frames++;}
static uint32_t time_us_32(void){return now_ms*1000;}
static int board_display_scan_row(uint32_t now){(void)now;return 490;}
static void sleep_us(unsigned n){(void)n;frames++;}
static void watchdog_update(void){}
static bool watchdog_caused_reboot(void){return false;}
static void *drive0_staging(void){return ram;}
static uint8_t *board_psram(void){return ram;}
static size_t board_psram_size(void){return sizeof ram;}
static const uint16_t *const *board_display_get_rows(void){return display;}
static void board_display_set_rows(const uint16_t *const *p){display=p;}
static void board_display_backlight(unsigned b){(void)b;}
static uint32_t board_display_frames(void){return frames;}
static const char *mock_machine="test";
static const char *machine_name(void){return mock_machine;}
static void machine_perf_get(unsigned *a,unsigned *b,unsigned *c){*a=90;*b=30;*c=100;}
static void __dmb(void){}
// BBC Micro has only 48,688 bytes free in the hardware regression sample.
// Reject large allocations so the old extra framebuffer cannot silently return.
static void *menu_test_malloc(size_t bytes){
#if MACHINE_BEEB
 assert(bytes<30000);
#else
#if MACHINE_MASTER
 assert(bytes<60000);
#endif
#endif
 return malloc(bytes);
}
#define malloc menu_test_malloc

''')
    if os.environ.get('MENU_PREVIEW_IMAGE_ID'):
        mock=d/'mock.h'
        image=os.environ['MENU_PREVIEW_IMAGE_ID']
        assert len(image)==16 and all(c in '0123456789abcdef' for c in image)
        mock.write_text(mock.read_text().replace('#define RETRO_IMAGE_ID "trial"', '#define RETRO_IMAGE_ID "'+image+'"'))
    (d/'test.c').write_text(r'''
#include <assert.h>
#include "subject.c"
static void step(bool pressed, uint32_t ms, uint32_t f){down=pressed;now_ms=ms;frames=f;boot_status_frame();}

// Exercise paging through the real frame/input path, with enough elapsed scanout
// frames to make every input observable on all four display implementations.
static void page_frame(unsigned elapsed_ms){step(false,now_ms+elapsed_ms,frames+4);}
static void page_start(unsigned count){
 static char names[240][24];
 if(status_visible)status_close();
 touching=false;joystick_online=keyboard_online=false;
 joystick_directions=keyboard_directions=0;joystick_fire=keyboard_fire=false;
 page_frame(40);sd_count=count;
 for(unsigned i=0;i<count;i++){
  snprintf(names[i],sizeof names[i],"Title%03u.tap",i);sd_names[i]=names[i];
 }
 titles_begin();assert(status_open());while(title_iterator)titles_poll();
 loaded[0]=0;load_ok=true;page_frame(40);
 assert(status_visible&&chooser&&title_count==count&&!loaded[0]);
}
static void page_tap(unsigned x,unsigned y){
 touching=false;page_frame(40);
 touch_x=x;touch_y=y;touching=true;page_frame(40);
 touching=false;page_frame(40);
}
static void file_page_tests(void){
 // Page edges clamp by displayed page, including short/empty lists. Keeping
 // an in-page highlight must not turn an unavailable page jump into a row move.
 const unsigned cases[][4]={
  {0,0,50,0},{0,0,240,0},{1,0,50,0},{1,0,240,0},
  {3,1,50,1},{3,1,240,1},{5,3,50,3},{5,3,240,3},
  {6,4,240,5},{6,5,240,5},{6,5,50,0},
  {8,2,240,7},{8,5,240,5},{8,7,50,2},
  {12,4,240,9},{12,9,240,11},{12,10,240,10},{12,11,50,6},
  {240,234,240,239},{240,238,240,238},{240,238,50,233}
 };
 for(unsigned i=0;i<sizeof cases/sizeof cases[0];i++){
  page_start(cases[i][0]);title_selected=cases[i][1];
  page_tap(cases[i][2],390);
  assert(title_selected==cases[i][3]&&status_visible&&!loaded[0]);
 }
 // Holding a touch makes one page jump; lifting and tapping makes the next.
 page_start(20);title_selected=1;touch_x=240;touch_y=390;touching=true;
 page_frame(40);assert(title_selected==6);
 page_frame(700);assert(title_selected==6&&status_visible&&!loaded[0]);
 touching=false;page_frame(40);touching=true;page_frame(40);assert(title_selected==11);
 touching=false;page_frame(40);
 // A partial final page has no selectable invisible entries. Tapping a real
 // filename after paging still boots that exact file, rather than its row index.
 page_start(12);title_selected=9;page_tap(240,390);assert(title_selected==11);
 page_tap(100,292);assert(title_selected==11&&status_visible&&!loaded[0]);
 page_tap(100,92);assert(!status_visible&&!strcmp(loaded,"Title010.tap"));
 // The BOOT button uses the retained highlight on the new page.
 page_start(8);title_selected=2;page_tap(240,390);assert(title_selected==7);
 page_tap(400,390);assert(!status_visible&&!strcmp(loaded,"Title007.tap"));
 // Nimbus and keyboard retain single-row Up/Down and gain Left/Right paging.
 // Direction changes are immediate; held input repeats only after its delay.
 for(unsigned keyboard_source=0;keyboard_source<2;keyboard_source++){
  page_start(20);title_selected=2;
  joystick_online=!keyboard_source;keyboard_online=keyboard_source;
  unsigned *directions=keyboard_source?&keyboard_directions:&joystick_directions;
  *directions=8;page_frame(1);assert(title_selected==7);
  page_frame(399);assert(title_selected==7);
  page_frame(1);assert(title_selected==12);
  page_frame(139);assert(title_selected==12);
  page_frame(1);assert(title_selected==17);
  *directions=4;page_frame(1);assert(title_selected==12);
  *directions=0;page_frame(1);*directions=4;page_frame(1);assert(title_selected==7);
  *directions=2;page_frame(1);assert(title_selected==8);
  *directions=1;page_frame(1);assert(title_selected==7);
  *directions=1|8;page_frame(400);assert(title_selected==6); // vertical priority
  *directions=4|8;page_frame(400);assert(title_selected==6); // opposing horizontal
  *directions=8;page_frame(1);assert(title_selected==11);
  *directions=0;page_frame(1);
  if(keyboard_source)keyboard_fire=true;else joystick_fire=true;
  page_frame(1);assert(!status_visible&&!captured&&!strcmp(loaded,"Title011.tap"));
 }
 // Opposing horizontal inputs from different devices cancel as well.
 page_start(12);title_selected=6;joystick_online=keyboard_online=true;
 joystick_directions=4;keyboard_directions=8;page_frame(1);assert(title_selected==6);
 keyboard_directions=0;page_frame(1);assert(title_selected==1);
 status_close();touching=false;joystick_online=keyboard_online=false;
 joystick_directions=keyboard_directions=0;joystick_fire=keyboard_fire=false;
}
int main(void){
 // Cold-start banner must fit its compact SRAM banks and restore the borrowed framebuffer.
 for(unsigned i=0;i<272*241;i++)sram_pixels[i]=0x13579bdf;
 boot_banner_show();
 assert(display==emulator&&!render_held&&now_ms>=3000);
 for(unsigned i=0;i<272*241;i++)assert(sram_pixels[i]==0x13579bdf);
 boot_button_t b={0};
 assert(!boot_button_press(&b,true,0)); // held on boot
 assert(!boot_button_press(&b,true,1000));
 assert(!boot_button_press(&b,false,1010));assert(!boot_button_press(&b,false,1050));
 assert(!boot_button_press(&b,true,1060));assert(!boot_button_press(&b,false,1070));
 assert(!boot_button_press(&b,true,1080));assert(boot_button_press(&b,true,1120));
 assert(!boot_button_press(&b,true,5000));
 b=(boot_button_t){0};assert(!boot_button_press(&b,false,UINT32_MAX-30));
 assert(!boot_button_press(&b,true,UINT32_MAX-20));assert(boot_button_press(&b,true,20));
 memset(ram,0xa5,sizeof ram);
 for(unsigned i=0;i<272*241;i++)sram_pixels[i]=(i%241==240)?0:0x12345678;
 step(false,0,100);step(true,100,105);step(true,140,107);
 assert(status_visible && display!=emulator && muted);
#if MACHINE_C64 || MACHINE_SPECTRUM
 assert(render_held);
 assert((const uint32_t *)display[0]>=sram_pixels);
#endif
 chooser=false;
 const uint16_t *const *first=display;
 for(unsigned i=0;i<sizeof ram-BOARD_STATUS_BYTES;i++)assert(ram[i]==0xa5);
 for(unsigned bank=status_active;bank<=status_active;bank++){
  const uint16_t **t=runtime_rows+bank*481;
  assert(t[480]==NULL);
  for(unsigned y=0;y<480;y++){
   assert(t[y]==t[y&~1u]);
   assert((const uint32_t *)t[y]>=status_sram+bank*STATUS_BANK_ROWS*BOARD_DISPLAY_ROW_WORDS);
   assert((const uint32_t *)t[y]<status_sram+(bank+1)*STATUS_BANK_ROWS*BOARD_DISPLAY_ROW_WORDS);
   const uint32_t *row=(const uint32_t *)t[y];
#if MACHINE_C64 || MACHINE_SPECTRUM
   assert(row[240]==0); // RGB565 padding, never packed-4 end marker
#else
   assert(row[60]==8); // packed-4 scanline sentinel
#endif
  }
 }
 // Solid red in the actual native encoding; boundary writes ignored.
 uint32_t scratch[BOARD_DISPLAY_ROW_WORDS*240]={0};
 pixel(scratch,0,0,4);
#if MACHINE_C64 || MACHINE_SPECTRUM
 assert(((uint16_t *)scratch)[0]==0x001f);
#else
 assert(scratch[0]==4);
#endif
 pixel(scratch,-1,0,7);pixel(scratch,480,0,7);pixel(scratch,0,240,7);
 step(true,1140,157);assert(display!=first); // refresh; held button doesn't toggle
 step(false,1160,158);step(false,1200,160);
 step(true,1220,161);step(true,1260,163);assert(!status_visible && display==emulator && !muted);
#if MACHINE_C64 || MACHINE_SPECTRUM
 assert(!render_held);
 for(unsigned i=0;i<272*241;i++)assert(sram_pixels[i]==((i%241==240)?0:0x12345678));
#endif
 step(false,1280,164);step(false,1320,166);
 step(true,1340,167);step(true,1380,169);assert(status_visible);
 boot_status_flash_begin();
#if MACHINE_C64 || MACHINE_SPECTRUM
 assert(display!=emulator && status_visible); // SRAM scanout needs no flash suspension
#else
 assert(display!=emulator && status_visible);
#endif
 boot_status_flash_end();assert(display!=emulator && status_visible);
 status_close();assert(!captured);
 // Eligible titles open the chooser; names sort, held fire cannot auto-load.
 joystick_online=true;joystick_fire=true;sd_count=3;
 sd_names[0]="Zeta.tap";sd_names[1]="Alpha.tap";sd_names[2]="Beta.tap";
 titles_begin();assert(chooser);status_open();assert(captured);
 step(false,2000,200);assert(title_count==3 && !strcmp(titles()[0],"Alpha.tap"));
 assert(status_visible && !loaded[0]);
 joystick_fire=false;step(false,2100,205);
 joystick_directions=2;step(false,2200,210);assert(title_selected==1);
 step(false,2300,215);assert(title_selected==1);
 step(false,2600,230);assert(title_selected==2);
 joystick_directions=0;joystick_fire=true;step(false,2700,235);
 assert(!status_visible && !captured && !strcmp(loaded,"Zeta.tap"));
 // Four BOOT presses: chooser, machines, status, emulator.
 joystick_fire=false;
 step(true,2800,240);step(true,2840,242);assert(chooser && status_visible && !touch_keyboard);
 step(false,2900,245);step(false,2940,247);
 step(true,3000,250);step(true,3040,252);assert(!chooser && machine_chooser && status_visible);
 step(false,3100,255);step(false,3140,257);
 step(true,3200,260);step(true,3240,262);assert(status_visible&&!machine_chooser);
 step(false,3250,264);step(false,3290,266);
 step(true,3300,268);step(true,3340,270);assert(!status_visible);
 // Failed load keeps a readable status page and an exit path.
 titles_begin();status_open();load_ok=false;
 step(false,3500,275);step(false,3540,277);joystick_fire=true;
 step(false,3600,280);assert(status_visible && !chooser && load_error);
 status_close();sd_count=0;titles_begin();assert(chooser);
 // A full chooser page must fit, and drawing the next screen leaves the live bank untouched.
 sd_count=8;for(unsigned i=0;i<8;i++)sd_names[i]="ABCDEFGHgjpqy.tap";
 titles_begin();status_open();titles_poll();
 status_active^=1;status_draw(status_active);status_present();
 assert(display==runtime_rows+status_active*481);
 uint32_t saved[STATUS_BANK_ROWS*BOARD_DISPLAY_ROW_WORDS];
 memcpy(saved,status_sram+status_active*STATUS_BANK_ROWS*BOARD_DISPLAY_ROW_WORDS,sizeof saved);
 unsigned live=status_active;status_active^=1;status_draw(status_active);status_present();
 assert(!memcmp(saved,status_sram+live*STATUS_BANK_ROWS*BOARD_DISPLAY_ROW_WORDS,sizeof saved));
 assert(display==runtime_rows+status_active*481);
 status_close();
 // Keyboard alone opens the chooser and Return loads the selected title.
 joystick_online=false;keyboard_online=true;keyboard_fire=false;
 titles_begin();assert(chooser);status_open();titles_poll();
 step(false,4000,400);
 keyboard_directions=2;step(false,4100,405);assert(title_selected==1);
 keyboard_directions=0;keyboard_fire=true;load_ok=true;
 step(false,4200,410);assert(!status_visible&&!captured);
 // Machine selection uses the same keyboard actions and switch command.
 keyboard_fire=false;machines_begin();assert(machine_chooser);status_open();
 machine_selected=0;step(false,4300,415);
 keyboard_directions=2;step(false,4400,420);assert(machine_selected==1);
 keyboard_directions=0;keyboard_fire=true;step(false,4500,425);
 assert(!status_visible&&!captured&&!strcmp(switched,"beeb"));
 // Held Return cannot immediately select on entering the next menu.
 machines_begin();status_open();step(false,4600,430);assert(status_visible);
 keyboard_fire=false;step(false,4700,435);switch_ok=false;
 keyboard_fire=true;step(false,4800,440);assert(status_visible&&!machine_chooser&&load_error);
 status_close();switch_ok=true;keyboard_online=false;keyboard_fire=false;
 joystick_online=true;joystick_fire=false;machines_begin();status_open();
 machine_selected=3;step(false,4900,445);
 joystick_directions=2;step(false,5000,450);assert(machine_selected==3);
 joystick_directions=1;step(false,5100,455);assert(machine_selected==2);
 joystick_directions=0;joystick_fire=true;step(false,5200,460);
 assert(!status_visible&&!captured&&!strcmp(switched,"c64"));
 // Touch opens the keyboard without delivering the opening tap; Settings opens titles.
 joystick_online=false;keyboard_online=false;joystick_fire=false;keyboard_fire=false;
 touching=true;touch_x=200;touch_y=200;step(false,5300,465);assert(status_visible&&chooser&&touch_keyboard);
 strcpy(last_command,"unchanged");step(false,5350,468);assert(touch_keyboard&&!strcmp(last_command,"unchanged"));
 touching=false;step(false,5400,470);
 touching=true;touch_x=400;touch_y=456;step(false,5440,472);
 assert(status_visible&&chooser&&!touch_keyboard&&!strcmp(last_command,"unchanged"));
 touching=false;step(false,5470,474);
 touching=true;touch_x=240;touch_y=456;step(false,5500,476);assert(machine_chooser);
 touching=false;step(false,5560,478);
 touching=true;touch_x=240;touch_y=280;step(false,5600,480);
 assert(!strcmp(last_command,"sound")&&!strcmp(last_args,"vol 50"));
 touching=false;step(false,5620,482);
 touching=true;touch_x=350;touch_y=380;step(false,5640,484);
 assert(!strcmp(last_command,"ambient")&&!strcmp(last_args,"bright 75"));
 touching=false;step(false,5660,486);
 touching=true;touch_x=50;touch_y=224;step(false,5680,488);
 assert(!strcmp(last_command,"sound")&&!strcmp(last_args,"off"));
 touching=false;step(false,5700,490);
 touching=true;touch_x=50;touch_y=324;step(false,5720,492);
 assert(!strcmp(last_command,"ambient")&&!strcmp(last_args,"off"));
 touching=false;step(false,5740,494);
 touching=true;touch_x=440;touch_y=456;step(false,5760,496);assert(!status_visible&&!captured);
 // Tapping a file boots it directly; holding the touch must not reopen the menu.
 touching=false;step(false,5800,500);touching=true;touch_x=200;touch_y=200;
 step(false,5840,502);assert(touch_keyboard);touch_key_activate(TK_SETTINGS);while(title_iterator)titles_poll();touching=false;step(false,5880,504);
 loaded[0]=0;touching=true;touch_x=100;touch_y=130;step(false,5920,506);
 assert(title_selected==1&&!status_visible&&loaded[0]);
 step(false,5940,520);assert(!status_visible);
 touching=false;step(false,5980,530);touching=true;touch_x=200;touch_y=200;
 step(false,6020,540);assert(touch_keyboard);touch_key_activate(TK_SETTINGS);while(title_iterator)titles_poll();touching=false;step(false,6060,550);loaded[0]=0;
 touching=true;touch_x=400;touch_y=390;
 step(false,6100,560);assert(!status_visible&&loaded[0]);
 // Every rendered row must reach scanout, even with filled button backgrounds.
 for(unsigned page=0;page<3;page++) {
   chooser=page==0;machine_chooser=page==1;title_count=8;title_selected=1;
#if MACHINE_MASTER
   if(getenv("MENU_PREVIEW_DIR")) {
     mock_machine="master";keyboard_online=true;title_count=5;
     const char *examples[]={"Arcadians.ssd","Elite.ssd","Frak.ssd","Repton3.ssd","Snapper.ssd"};
     for(unsigned i=0;i<5;i++)snprintf(titles()[i],TITLE_NAME_BYTES,"%s",examples[i]);
   }
#endif
   status_draw(status_active);status_present();
   const uint32_t *staged=(const uint32_t *)(board_psram()+board_psram_size()-BOARD_STATUS_BYTES)+BANNER_ROWS*BOARD_DISPLAY_ROW_WORDS;
   for(unsigned y=0;y<240;y++)assert(!memcmp(display[y*2],staged+y*BOARD_DISPLAY_ROW_WORDS,ROW_BYTES));
#if MACHINE_MASTER
   const char *dir=getenv("MENU_PREVIEW_DIR");
   if(dir){char path[512];snprintf(path,sizeof path,"%s/menu-%u.ppm",dir,page);FILE *f=fopen(path,"wb");assert(f);fprintf(f,"P6\n480 480\n255\n");
     for(unsigned y=0;y<480;y++)for(unsigned x=0;x<480;x++){unsigned c=(((const uint32_t *)display[y])[x/8]>>((x%8)*4))&7;unsigned char rgb[3]={c&4?255:0,c&2?255:0,c&1?255:0};fwrite(rgb,1,3,f);}fclose(f);}
#endif
 }

 // Every setup page and all key-picker pages fit the immutable scanout banks.
 chooser=machine_chooser=false;status_visible=true;bt_state.devices=32;bt_state.remembered=true;
 strcpy(bt_state.address,"11:22:33:44:55:66");
 for(unsigned i=0;i<32;i++){bt_map[i*2]=i+4;bt_map[i*2+1]=setup_keys[i%(SETUP_KEY_COUNT)].code;}
 setup_defaults();
 for(unsigned keyboard=0;keyboard<2;keyboard++)for(unsigned page=SETUP_HOME;page<=SETUP_FORGET;page++) {
   setup_keyboard=keyboard;setup_go(page);
   unsigned pages=page==SETUP_TARGET?12:1;
   for(unsigned offset=0;offset<pages;offset++) {
     setup_offset=offset*12;status_active^=1;status_draw(status_active);status_present();
     const uint32_t *staged=(const uint32_t *)(board_psram()+board_psram_size()-BOARD_STATUS_BYTES)+BANNER_ROWS*BOARD_DISPLAY_ROW_WORDS;
     for(unsigned y=0;y<240;y++)assert(!memcmp(display[y*2],staged+y*BOARD_DISPLAY_ROW_WORDS,ROW_BYTES));
#if MACHINE_MASTER
     const char *dir=getenv("MENU_PREVIEW_DIR");
     if(dir&&offset==0){char path[512];snprintf(path,sizeof path,"%s/setup-%u-%u.ppm",dir,keyboard,page);FILE *f=fopen(path,"wb");assert(f);fprintf(f,"P6\n480 480\n255\n");
       for(unsigned y=0;y<480;y++)for(unsigned x=0;x<480;x++){unsigned c=(((const uint32_t *)display[y])[x/8]>>((x%8)*4))&7;unsigned char rgb[3]={c&4?255:0,c&2?255:0,c&1?255:0};fwrite(rgb,1,3,f);}fclose(f);}
#endif

   }
 }
 // Scan/select must not connect during inquiry, or connect a replaced result.
 setup_open(true);setup_activate(SA_SCAN,0);assert(setup_page==SETUP_SCAN&&!strcmp(bt_command,"keyboard-scan"));
 setup_activate(SA_DEVICE,2);bt_state.scanning=true;bt_command[0]=0;setup_activate(SA_CONNECT,0);assert(!bt_command[0]);
 bt_state.scanning=false;setup_activate(SA_CONNECT,0);assert(!strcmp(bt_command,"keyboard connect 11:22:33:44:55:02")&&setup_page==SETUP_HOME);
 setup_go(SETUP_SCAN);strcpy(setup_address,"00:00:00:00:00:00");bt_command[0]=0;setup_activate(SA_CONNECT,0);assert(!bt_command[0]);
 // Capture uses the real command path, ends on capture/cancel, and maps the captured usage.
 setup_activate(SA_ADD,0);assert(setup_page==SETUP_CAPTURE&&bt_state.capturing);
 bt_state.captured_usage=67;setup_frame(false,false,0,0,10000);
 assert(setup_page==SETUP_TARGET&&setup_usage==67&&!bt_state.capturing);
 setup_activate(SA_TARGET,32);assert(!strcmp(bt_command,"keyboard map 67 32")&&setup_page==SETUP_KEY_MAP);
 setup_activate(SA_MAP_ROW,4);setup_activate(SA_DELETE,0);assert(!strcmp(bt_command,"keyboard unmap 6"));
 setup_activate(SA_ADD,0);setup_activate(SA_BACK,0);assert(!bt_state.capturing&&setup_page==SETUP_KEY_MAP);
 setup_activate(SA_ADD,0);bt_state.capturing=false;setup_frame(false,false,0,0,10001);assert(setup_page==SETUP_KEY_MAP);
 // Joystick draft changes do not save until Apply, and support all web fields.
 setup_open(false);setup_activate(SA_CONTROLS,0);bt_command[0]=0;
 setup_activate(SA_MODE,0);setup_activate(SA_PORT,0);setup_activate(SA_SOURCE,0);setup_activate(SA_IX,0);setup_activate(SA_IY,0);
 setup_activate(SA_DEAD,100);assert(setup_config[3]==50&&!bt_command[0]);setup_activate(SA_DEAD,-100);assert(setup_config[3]==0);
 setup_activate(SA_JOY_KEY,12);setup_activate(SA_TARGET,-2);assert(setup_config[18]==-2&&setup_page==SETUP_JOY_MAP);
 setup_activate(SA_APPLY,0);assert(!strncmp(bt_command,"config 2 1 1 0 1 1 ",19));
 // Busy/error responses stay on-screen; never pretend a failed save succeeded.
 bt_result=500;setup_activate(SA_APPLY,0);assert(strstr(setup_message,"COULD NOT"));bt_result=200;
 // Keyboard/Nimbus focus is edge-triggered; a held Return cannot repeat actions.
 setup_open(false);status_draw(status_active);keyboard_fire=false;keyboard_directions=2;
 setup_frame(false,false,0,0,11000);assert(setup_focus==1);keyboard_directions=0;
 setup_frame(false,false,0,0,11001);keyboard_fire=true;setup_frame(false,false,0,0,11002);assert(!strcmp(bt_command,"disconnect"));
 strcpy(bt_command,"unchanged");setup_frame(false,false,0,0,11003);assert(!strcmp(bt_command,"unchanged"));keyboard_fire=false;

 setup_end();status_close();
#if MACHINE_C64
 mock_machine="c64";
#elif MACHINE_SPECTRUM
 mock_machine="spectrum";
#elif MACHINE_BEEB
 mock_machine="beeb";
#else
 mock_machine="master";
#endif
 // Render every physical-key page and modifier state through the real bank encoder.
 titles_begin();status_open();touch_key_open();
 for(unsigned extra=0;extra<2;extra++)for(unsigned shift=0;shift<2;shift++)for(unsigned symbol=0;symbol<2;symbol++) {
   touch_key_extra=extra;touch_key_shift=shift;touch_key_symbol=symbol;
   status_active^=1;status_draw(status_active);status_present();
   const uint32_t *staged=(const uint32_t *)(board_psram()+board_psram_size()-BOARD_STATUS_BYTES)+BANNER_ROWS*BOARD_DISPLAY_ROW_WORDS;
   for(unsigned y=0;y<240;y++)assert(!memcmp(display[y*2],staged+y*BOARD_DISPLAY_ROW_WORDS,ROW_BYTES));
   for(unsigned i=0;i<touch_key_count;i++){touch_key_button_t b=touch_key_buttons[i];assert(b.x>=0&&b.y>=0&&b.x+b.w<=480&&b.y+b.h<=240);}
   const char *dir=getenv("MENU_PREVIEW_DIR");
   if(dir&&!symbol){char path[512];snprintf(path,sizeof path,"%s/keys-%s-%u-%u.ppm",dir,mock_machine,extra,shift);FILE *f=fopen(path,"wb");assert(f);fprintf(f,"P6\n480 480\n255\n");
     for(unsigned y=0;y<480;y++)for(unsigned x=0;x<480;x++){unsigned c;
#if MACHINE_C64 || MACHINE_SPECTRUM
       unsigned pins=((const uint16_t *)(staged+(y/2)*BOARD_DISPLAY_ROW_WORDS))[x];c=(pins&31?4:0)|(pins&0x7e0?2:0)|(pins&0xf800?1:0);
#else
       c=(staged[(y/2)*BOARD_DISPLAY_ROW_WORDS+x/8]>>((x%8)*4))&7;
#endif
unsigned char rgb[3]={c&4?255:0,c&2?255:0,c&1?255:0};fwrite(rgb,1,3,f);}fclose(f);}
 }
 // State fill is independent of focus: Shift off stays black, active page cyan.
 touch_key_open();status_draw(status_active);
 touch_key_focus=touch_key_count-2;status_draw(status_active);
 const uint32_t *key_pixels=(const uint32_t *)(board_psram()+board_psram_size()-BOARD_STATUS_BYTES)+BANNER_ROWS*BOARD_DISPLAY_ROW_WORDS;
#if MACHINE_C64 || MACHINE_SPECTRUM
 #define KEY_COLOUR(x,y) (((const uint16_t *)(key_pixels+(y)*BOARD_DISPLAY_ROW_WORDS))[x])
 #define CYAN_PIXEL 0xffe0
#else
 #define KEY_COLOUR(x,y) ((key_pixels[(y)*BOARD_DISPLAY_ROW_WORDS+(x)/8]>>(((x)%8)*4))&7)
 #define CYAN_PIXEL 3
#endif
 assert(KEY_COLOUR(170,218)==0);
 touch_key_activate(TK_SHIFT);status_draw(status_active);assert(KEY_COLOUR(170,218)==CYAN_PIXEL);
 touch_key_activate(TK_SHIFT);status_draw(status_active);assert(KEY_COLOUR(170,218)==0);
#if !MACHINE_SPECTRUM
 touch_key_activate(TK_EXTRA);status_draw(status_active);
 assert(KEY_COLOUR(16,22)==0&&KEY_COLOUR(250,22)==CYAN_PIXEL);
 touch_key_activate(TK_MAIN);status_draw(status_active);
 assert(KEY_COLOUR(16,22)==CYAN_PIXEL&&KEY_COLOUR(250,22)==0);
#endif
 #undef KEY_COLOUR
 #undef CYAN_PIXEL
 // Integration: Return on the keyboard tab must not boot an SD title underneath.
 touch_key_open();status_draw(status_active);touch_key_focus=touch_key_count-2;keyboard_fire=false;keyboard_directions=0;touching=false;
 step(false,11800,700);keyboard_fire=true;loaded[0]=0;step(false,11900,710);
 assert(status_visible&&touch_keyboard&&!loaded[0]);keyboard_fire=false;
 touch_key_open();status_draw(status_active);last_command[0]=0;
 touch_key_activate(TK_SHIFT);assert(touch_key_shift&&status_visible&&!last_command[0]);
 touch_key_activate(TK_SHIFT);assert(!touch_key_shift&&status_visible);
 // Cursor navigation moves focus; Return selects only after being released.
 keyboard_directions=8;keyboard_fire=true;touch_key_frame(false,0,0,12000);
 assert(touch_key_focus==1&&status_visible);keyboard_directions=0;
 keyboard_fire=false;touch_key_frame(false,0,0,12001);
 // Select a real key by touch, sending exactly one shifted press after resuming.
 touch_key_activate(TK_SHIFT);status_draw(status_active);
 unsigned selected=0;while(touch_key_buttons[selected].code<0)selected++;
 touch_key_button_t key=touch_key_buttons[selected];char expected[64];
#if MACHINE_C64
 unsigned modifier=15;
#else
 unsigned modifier=0;
#endif
 snprintf(expected,sizeof expected,"d%x d%x u%x u%x",modifier,key.code,key.code,modifier);
 touch_key_frame(true,key.x+2,key.y+2,12002);
 assert(!status_visible&&!touch_keyboard&&!strcmp(last_command,"keys")&&!strcmp(last_args,expected));
 // Settings returns to titles without delivering a key; Reset queues the standard reset.
 titles_begin();status_open();touch_key_open();strcpy(last_command,"unchanged");
 touch_key_activate(TK_SETTINGS);assert(status_visible&&chooser&&!touch_keyboard&&!strcmp(last_command,"unchanged"));
 touch_key_open();touch_key_activate(TK_RESET);assert(!status_visible&&!strcmp(last_args,"b"));
#if MACHINE_SPECTRUM
 titles_begin();status_open();touch_key_open();touch_key_activate(TK_SYMBOL);touch_key_activate(5);
 assert(!strcmp(last_args,"d24 d5 u5 u24"));
#endif
 status_visible=false;
 file_page_tests();

 launched=false;machines_begin();assert(!machine_chooser);
#if !BORROW_STATUS_FB
 free(status_sram);
#endif
 free(runtime_rows);puts("button, buffer bounds, row encoding and restore: PASS");
}
''')
    for machine in ['MASTER','BEEB','C64','SPECTRUM']:
        subprocess.run(['cc','-std=c11','-fsanitize=address,undefined','-g',f'-DMACHINE_{machine}=1','-I',tmp,str(d/'test.c'),'-o',str(d/'test')],check=True)
        print(machine,flush=True);subprocess.run([str(d/'test')],check=True)
