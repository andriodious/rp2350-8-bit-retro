#!/usr/bin/env python3
"""Run the firmware bridge and patched Frodo lifecycle on the host."""
import argparse, os, subprocess, sys
from pathlib import Path
HERE=Path(__file__).resolve().parent;ROOT=HERE.parents[2]
p=argparse.ArgumentParser();p.add_argument('--trx64',required=True);p.add_argument('--image',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
p.add_argument('--idle-sram',action='store_true')
p.add_argument('--drive-profile',action='store_true')
p.add_argument('--crt',type=Path)
p.add_argument('--benchmark',action='store_true')
p.add_argument('--reuse-prepared',action='store_true')
p.add_argument('--flush-lines',type=int,choices=[1,4,8,16],default=1)
p.add_argument('--bus-trace',action='store_true')
p.add_argument('--sid-fast-state',action='store_true')
p.add_argument('--sid-hot-code',action='store_true')
p.add_argument('--delay-loop',action='store_true')
p.add_argument('--job-scan',action='store_true')
p.add_argument('--serial-wait',action='store_true')
p.add_argument('--drive-pc-profile',action='store_true',help='Host-only drive instruction histogram; changes timing')
a=p.parse_args();out=a.output.resolve()
if not a.reuse_prepared: subprocess.run([sys.executable,str(HERE/'prepare.py'),'--output',str(out),'--trx64',a.trx64,'--flush-lines',str(a.flush_lines),*(['--idle-sram'] if a.idle_sram else []),*(['--drive-profile'] if a.drive_profile else []),*(['--sid-fast-state'] if a.sid_fast_state else []),*(['--sid-hot-code'] if a.sid_hot_code else []),*(['--delay-loop'] if a.delay_loop else []),*(['--job-scan'] if a.job_scan else []),*(['--serial-wait'] if a.serial_wait else [])],check=True)
if a.drive_pc_profile:
 # Single emulation thread only; never included in hardware firmware.
 pc_source=out/'portable/src/drive_6510core.rs'
 t=pc_source.read_text()
 t+='\nstatic mut HOST_PC_COUNTS: [u64;65536] = [0;65536];\n#[no_mangle] pub extern "C" fn host_drive_pc_count(pc:u16)->u64 {unsafe{HOST_PC_COUNTS[pc as usize]}}\n'
 t=t.replace('    let reg_pc0 = ex.core.reg_pc;', '    unsafe { HOST_PC_COUNTS[ex.core.reg_pc as usize] += 1; }\n    let reg_pc0 = ex.core.reg_pc;')
 pc_source.write_text(t)
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
if a.serial_wait:
 s=s.replace('assert(retro_drive_idle_loops()>0);', 'assert(retro_drive_idle_loops()>0);assert(retro_drive_serial_loops()+retro_drive_serial_tail_instructions()>0);printf("SERIAL loops=%llu tails=%llu\\n",(unsigned long long)retro_drive_serial_loops(),(unsigned long long)retro_drive_serial_tail_instructions());')
s=s.replace('void c64_init();', 'const char *c64_crt_prepare(const uint8_t *,uint32_t);void c64_crt_detach();\nvoid c64_init();')
s=s.replace(' const size_t baseline=live;', '''
 if(!strcmp(argv[4],"crt")){
  assert(!c64_crt_prepare(disk.data(),disk.size()));c64_reset();
  for(int i=1;i<=10000;i++){frames(1);if(i%500==0){
   std::string p=std::string(argv[3])+"/crt-frame-"+std::to_string(i)+".raw";
   FILE *f=fopen(p.c_str(),"wb");assert(f);fwrite(c64_get_framebuffer(),1,384*272,f);fclose(f);
  }}
  assert(!retro_drive_active());c64_crt_detach();puts("CRT 10000 frames passed");return 0;
 }
 const size_t baseline=live;''')
s=s.replace(' for(int i=1;i<=10000;i++){frames(1);if(i%500==0){', ' auto bench_start=std::chrono::steady_clock::now();\n for(int i=1;i<=10000;i++){frames(1);if(i%500==0){\n auto bench_now=std::chrono::steady_clock::now();printf("BENCH frame=%d us=%lld drive_us=%llu\\n",i,(long long)std::chrono::duration_cast<std::chrono::microseconds>(bench_now-bench_start).count(),(unsigned long long)retro_drive_run_us());')
s=s.replace('fwrite(c64_get_framebuffer(),1,384*272,f);fclose(f);', 'fwrite(c64_get_framebuffer(),1,384*272,f);fclose(f);{FILE *rf=fopen((p+".ram").c_str(),"wb");assert(rf);fwrite(c64_get_ram(),1,65536,rf);fclose(rf);}')
if a.bus_trace:
 s=s.replace('static uint64_t net_service_calls;', '''static uint64_t bus_hash=1469598103934665603ULL,bus_reads,bus_writes;
 static void bus_byte(uint8_t b){bus_hash=(bus_hash^b)*1099511628211ULL;}
 uint8_t observed_drive_read(){uint8_t b=retro_drive_read();bus_byte(0);bus_byte(b);bus_reads++;return b;}
 void observed_drive_write(uint8_t b){bus_byte(1);bus_byte(b);bus_writes++;retro_drive_write(b);}
 static uint64_t net_service_calls;''')
 s=s.replace('assert(argc==5);', 'atexit([](){printf("BUS reads=%llu writes=%llu hash=%016llx\\n",(unsigned long long)bus_reads,(unsigned long long)bus_writes,(unsigned long long)bus_hash);});assert(argc==5);')
 p=out/'core/CIA.cpp';t=p.read_text()
 t=t.replace('retro_drive_read()', 'observed_drive_read()').replace('retro_drive_write(', 'observed_drive_write(')
 t='#include <stdint.h>\nextern "C" uint8_t observed_drive_read();\nextern "C" void observed_drive_write(uint8_t);\n'+t;p.write_text(t)
s=s.replace('void sid_add_sample(int16_t,int16_t){}', '''static uint64_t audio_hash=1469598103934665603ULL,audio_samples;
void sid_add_sample(int16_t l,int16_t r){for(uint16_t v:{(uint16_t)l,(uint16_t)r}){audio_hash=(audio_hash^(v&255))*1099511628211ULL;audio_hash=(audio_hash^(v>>8))*1099511628211ULL;}audio_samples++;}
static size_t sid_live,sid_peak;
void *c64_sid_state_alloc(size_t n){assert(sid_live==0);void *p=malloc(n);assert(p);sid_live=n;if(n>sid_peak)sid_peak=n;return p;}
void c64_sid_state_free(void *p){assert(sid_live);free(p);sid_live=0;}
''')
s=s.replace('assert(argc==5);', 'atexit([](){printf("PCM samples=%llu hash=%016llx sid_live=%zu sid_peak=%zu\\n",(unsigned long long)audio_samples,(unsigned long long)audio_hash,sid_live,sid_peak);});assert(argc==5);')
# Exercise the actual virtual destructor/class-specific delete after the game.
p=out/'core/rp2350/C64_rp2350.cpp'
p.write_text(p.read_text()+'''
extern "C" void c64_sid_lifecycle_probe() {
    Prefs saved=ThePrefs,off=ThePrefs;off.SIDType=SIDTYPE_NONE;
    for(int i=0;i<20;i++) {
        TheC64->TheSID->NewPrefs(&off);ThePrefs=off;
        TheC64->TheSID->NewPrefs(&saved);ThePrefs=saved;
        TheC64->TheSID->Reset();
    }
    TheC64->TheSID->NewPrefs(&off);ThePrefs=off;
}
''')
s=s.replace('void c64_init();', 'void c64_sid_lifecycle_probe();\nvoid c64_init();')
s=s.replace('sid_live,sid_peak);});assert(argc==5);', 'sid_live,sid_peak);c64_sid_lifecycle_probe();assert(sid_live==0);puts("SID renderer lifecycle passed");});assert(argc==5);')
if a.drive_pc_profile:
 s='extern "C" unsigned long long host_drive_pc_count(unsigned short);\n'+s
 s=s.replace('assert(argc==5);', 'atexit([](){for(unsigned pc=0;pc<65536;pc++){auto n=host_drive_pc_count(pc);if(n)printf("DRIVE_PC %04x %llu\\n",pc,n);}});assert(argc==5);')
(out/'main.cpp').write_text(s)
rom=ROOT/'roms/1581/dos1581-318045-02.bin'
subprocess.run([sys.executable,str(ROOT/'src/machines/master/embed.py'),str(out/'rom.c'),'retro_1581_rom='+str(rom)],check=True)
host=ROOT/'tests/c64/native';machine=ROOT/'src/machines/c64';core=out/'core'
objects=[]
for f in [machine/'disk_file.c',machine/'t64.c',out/'rom.c']:
 obj=out/(f.stem+'.o');objects.append(obj)
 subprocess.run(['cc',* ([] if a.benchmark else ['-fsanitize=address,undefined']),'-I'+str(machine),'-c',str(f),'-o',str(obj)],check=True)
sources=[out/'main.cpp',host/'arch.cpp',machine/'display.cpp',machine/'crt.cpp',*objects]
sources += [core/'rp2350'/(n+'.cpp') for n in ['C64_rp2350','Prefs_rp2350','ROM_data','SID_rp2350','Tape_stub']]
sources += [core/(n+'.cpp') for n in ['CPUC64','VIC','CIA','IEC','Cartridge','REU','1541d64','1541gcr','CPU1541','VIA']]
subprocess.run(['c++','-std=c++20',*(['-O2'] if a.benchmark else ['-O1','-g','-fsanitize=address,undefined']),'-DFRODO_RP2350','-DC64_1581_NET_SLICES=1','-DPSRAM_MAX_FREQ_MHZ=125',*['-I'+str(d) for d in [HERE,host,machine,core,core/'rp2350']],*map(str,sources),str(out/'rust/target/release/libretro_1581_firmware.a'),'-o',str(out/'test')],check=True)
env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1')
for mode in ['probe','game']:subprocess.run([str(out/'test'),str(rom),str(a.image),str(out),mode],env=env,check=True)

if a.crt: subprocess.run([str(out/"test"),str(rom),str(a.crt),str(out),"crt"],env=env,check=True)
