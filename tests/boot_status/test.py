#!/usr/bin/env python3
"""Run the real status renderer/toggle code with mocked board IO, under sanitizers."""
from pathlib import Path
import subprocess, tempfile
root = Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory() as tmp:
    d=Path(tmp)
    source=(root/'src/common/boot_banner.c').read_text()
    # Host replacement for the one ARM-only pixel bit reversal instruction.
    source=source.replace('__asm("rbit %0, %1" : "=r"(pins) : "r"(rgb << 16));',
                          'pins = 0; for (unsigned bit=0; bit<16; bit++) pins |= ((rgb >> bit) & 1u) << (15-bit);')
    (d/'subject.c').write_text(source)
    for name in ['boot_button.h','boot_logo.h']:
        (d/name).write_text((root/'src/common'/name).read_text())
    for name in ['boot_banner.h','board.h','drive.h','machine_if.h','net_if.h','pico/stdlib.h','hardware/watchdog.h','hardware/sync.h','firmware_identity.h','sound.h']:
        p=d/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_text('#include "mock.h"\n')
    (d/'mock.h').write_text(r'''
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define BOOT_STATUS_TRIAL 1
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
#define NIMBUS_TEST 0
#define RETRO_IMAGE_ID "trial"
#define GPIO_IN 0
static uint8_t ram[8u*1024u*1024u];
static const uint16_t *emulator[481];
static const uint16_t *const *display=emulator;
static uint32_t now_ms, frames=100;
static bool down,muted,render_held;
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
static void watchdog_update(void){}
static bool watchdog_caused_reboot(void){return false;}
static void *drive0_staging(void){return ram;}
static uint8_t *board_psram(void){return ram;}
static size_t board_psram_size(void){return sizeof ram;}
static const uint16_t *const *board_display_get_rows(void){return display;}
static void board_display_set_rows(const uint16_t *const *p){display=p;}
static void board_display_backlight(unsigned b){(void)b;}
static uint32_t board_display_frames(void){return frames;}
static const char *machine_name(void){return "test";}
static void machine_perf_get(unsigned *a,unsigned *b,unsigned *c){*a=90;*b=30;*c=100;}
static void __dmb(void){}
''')
    (d/'test.c').write_text(r'''
#include <assert.h>
#include "subject.c"
static void step(bool pressed, uint32_t ms, uint32_t f){down=pressed;now_ms=ms;frames=f;boot_status_frame();}
int main(void){
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
 assert(display[0]==(const uint16_t *)sram_pixels);
#endif
 const uint16_t *const *first=display;
 for(unsigned i=0;i<sizeof ram-BOARD_STATUS_BYTES;i++)assert(ram[i]==0xa5);
 for(unsigned bank=0;bank<2;bank++){
  const uint16_t **t=runtime_rows+bank*481;
  assert(t[480]==NULL);
  for(unsigned y=0;y<480;y++){
   assert(t[y]==t[y&~1u]);
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
 assert(display==emulator && status_visible);
#endif
 boot_status_flash_end();assert(display!=emulator && status_visible);
 free(runtime_rows);puts("button, buffer bounds, row encoding and restore: PASS");
}
''')
    for machine in ['MASTER','BEEB','C64','SPECTRUM']:
        subprocess.run(['cc','-std=c11','-fsanitize=address,undefined','-g',f'-DMACHINE_{machine}=1','-I',tmp,str(d/'test.c'),'-o',str(d/'test')],check=True)
        print(machine,flush=True);subprocess.run([str(d/'test')],check=True)
