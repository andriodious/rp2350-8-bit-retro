#!/usr/bin/env python3
"""Run the firmware bridge and patched Frodo lifecycle on the host."""
import argparse, os, subprocess, sys
from pathlib import Path
HERE=Path(__file__).resolve().parent;ROOT=HERE.parents[2]
p=argparse.ArgumentParser();p.add_argument('--trx64',required=True);p.add_argument('--image',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
p.add_argument('--idle-sram',action='store_true')
a=p.parse_args();out=a.output.resolve()
subprocess.run([sys.executable,str(HERE/'prepare.py'),'--output',str(out),'--trx64',a.trx64,*(['--idle-sram'] if a.idle_sram else [])],check=True)
subprocess.run(['cargo','build','--release','--manifest-path',str(out/'rust/Cargo.toml')],check=True)
s=(HERE.parent/'bridge/main.cpp').read_text().replace('#include <cassert>','#include <cassert>\n#include <chrono>')
s=s.replace('void *psram_malloc(size_t n){return malloc(n);}void psram_free(void *p){free(p);}void *psram_realloc(void *p,size_t n){return realloc(p,n);}', '''static size_t live,peak;
void *psram_malloc(size_t n){size_t *p=(size_t*)malloc(n+16);if(!p)return nullptr;*p=n;live+=n;if(live>peak)peak=live;return (char*)p+16;}
void psram_free(void *p){if(!p)return;size_t *h=(size_t*)((char*)p-16);live-=*h;free(h);}
void *psram_realloc(void *p,size_t n){if(!p)return psram_malloc(n);void *q=psram_malloc(n);if(q){size_t old=*(size_t*)((char*)p-16);memcpy(q,p,n<old?n:old);psram_free(p);}return q;}
''')
s=s.replace('void c64_init();','uint32_t retro_drive_time_us(){return (uint32_t)std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();} void retro_drive_panic(){abort();} void rust_eh_personality(){abort();}\nvoid c64_mount_disk(const uint8_t *,uint32_t,const char *);void c64_unmount_disk();void c64_reset();void c64_disk_set(uint8_t *,size_t);\nvoid c64_init();')
s=s.replace('assert(retro_drive_init(rom.data(),rom.size(),disk.data(),disk.size()));c64_init();frames(1000);', '''c64_init();
 const size_t baseline=live;
 // Exercise repeated disk selection, full-drive reset and fallback without leaking drive allocations.
 for(int i=0;i<3;i++){
  c64_disk_set(disk.data(),disk.size());c64_mount_disk(disk.data(),disk.size(),"presto.d64");assert(retro_drive_active());c64_reset();frames(5);
  assert(net_service_calls>0);const auto calls=net_service_calls;
  c64_unmount_disk();assert(!retro_drive_active());c64_reset();frames(5);assert(live==baseline);assert(net_service_calls==calls);
 }
 c64_disk_set(disk.data(),disk.size());c64_mount_disk(disk.data(),disk.size(),"presto.d64");assert(retro_drive_active());c64_reset();frames(250);
 printf("1581 PSRAM live=%zu peak=%zu baseline=%zu\\n",live,peak,baseline);''')
s=s.replace('printf("frame=%d drive_pc=%04x\\n",i,retro_drive_pc());', 'printf("frame=%d drive_pc=%04x idle_loops=%llu\\n",i,retro_drive_pc(),(unsigned long long)retro_drive_idle_loops());if(i==10000){assert(retro_drive_idle_loops()>0);assert(retro_drive_idle_tail_instructions()>0);printf("idle_tail_instructions=%llu\\n",(unsigned long long)retro_drive_idle_tail_instructions());}')
(out/'main.cpp').write_text(s)
rom=ROOT/'roms/1581/dos1581-318045-02.bin'
subprocess.run([sys.executable,str(ROOT/'src/machines/master/embed.py'),str(out/'rom.c'),'retro_1581_rom='+str(rom)],check=True)
host=ROOT/'tests/c64/native';machine=ROOT/'src/machines/c64';core=out/'core'
objects=[]
for f in [machine/'disk_file.c',machine/'t64.c',out/'rom.c']:
 obj=out/(f.stem+'.o');objects.append(obj)
 subprocess.run(['cc','-fsanitize=address,undefined','-I'+str(machine),'-c',str(f),'-o',str(obj)],check=True)
sources=[out/'main.cpp',host/'arch.cpp',machine/'display.cpp',machine/'crt.cpp',*objects]
sources += [core/'rp2350'/(n+'.cpp') for n in ['C64_rp2350','Prefs_rp2350','ROM_data','SID_rp2350','Tape_stub']]
sources += [core/(n+'.cpp') for n in ['CPUC64','VIC','CIA','IEC','Cartridge','REU','1541d64','1541gcr','CPU1541','VIA']]
subprocess.run(['c++','-std=c++20','-O1','-g','-fsanitize=address,undefined','-DFRODO_RP2350','-DC64_1581_NET_SLICES=1','-DPSRAM_MAX_FREQ_MHZ=125',*['-I'+str(d) for d in [HERE,host,machine,core,core/'rp2350']],*map(str,sources),str(out/'rust/target/release/libretro_1581_firmware.a'),'-o',str(out/'test')],check=True)
env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1')
for mode in ['probe','game']:subprocess.run([str(out/'test'),str(rom),str(a.image),str(out),mode],env=env,check=True)
