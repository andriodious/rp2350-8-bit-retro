#!/usr/bin/env python3
"""Execute launcher selection/return capture with mocked ROM calls, not hardware."""
from pathlib import Path
import re, subprocess, tempfile
root = Path(__file__).resolve().parents[2]
s = (root/'src/launcher/launcher.c').read_text()
s = re.sub(r'^#include .*$', '', s, flags=re.M)
a=s.index('static void __attribute__((noreturn)) report_fallback(void) {')
b=s.index('\n#endif',a)
s=s[:a]+'static void __attribute__((noreturn)) report_fallback(void) { longjmp(done, 2); }'+s[b:]
s=s.replace('int main(void)', 'int launcher_main(void)')
pre=r'''
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <assert.h>
#include <setjmp.h>
#define LAUNCHER_BOOT_REPORT 1
#define STARTUP_DIAG 1
#define __aligned(x) __attribute__((aligned(x)))
#define XIP_BASE 0x10000000u
#define FLASH_SECTOR_SIZE 4096u
#define PT_INFO_PARTITION_LOCATION_AND_FLAGS 1
#define PT_INFO_SINGLE_PARTITION 2
#define PICOBIN_PARTITION_LOCATION_FIRST_SECTOR_BITS 0x1fff
#define PICOBIN_PARTITION_LOCATION_FIRST_SECTOR_LSB 0
#define PICOBIN_PARTITION_LOCATION_LAST_SECTOR_BITS 0x1fff0000
#define PICOBIN_PARTITION_LOCATION_LAST_SECTOR_LSB 16
#define SYSTEM_SLOT_MASTER 0
#define SYSTEM_SLOT_BEEB 1
#define SYSTEM_SLOT_C64 2
#define C64_DIAG_MAGIC 0xc6400000u
#define C64_RETURN_MAGIC 0xc6500000u
#define BOARD_FLASH_SYSTEM_OFFSET 0
#define XIP_NOCACHE_NOALLOC_BASE ((uintptr_t)&settings)
typedef int system_settings_t;
static int settings;
static struct {uint32_t scratch[8];} hw;
#define watchdog_hw (&hw)
typedef struct {uint32_t boot_word,boot_diagnostic,reboot_params[2];} boot_info_t;
static jmp_buf done;
static int lookups[3], chains[3], calls, success_slot=-1, load_rc;
static bool watchdog_caused_reboot(void){return false;}
static bool watchdog_enable_caused_reboot(void){return false;}
static int system_slot(int x){return x;}
static int system_settings_machine(const int *p){return *p;}
static void startup_diag_mark(unsigned x){(void)x;}
static bool rom_get_boot_info(boot_info_t *p){p->boot_word=123;return true;}
int partition_diag_load(uint8_t *p,uint32_t n){(void)p;(void)n;return load_rc;}
static int rom_get_partition_table_info(uint32_t *p,int n,uint32_t flags){
 int slot=flags>>24; (void)n; p[1]=(uint32_t)(slot+2)<<16 | (slot+1); return lookups[slot];
}
static int rom_chain_image(uint8_t *p,uint32_t n,uint32_t start,uint32_t size){
 (void)p;(void)n;(void)size; int slot=(start-XIP_BASE)/4096-1; ++calls;
 if(slot==success_slot)longjmp(done,1); return chains[slot];
}
'''
post=r'''
int main(void){
 lookups[0]=3;lookups[1]=-10;lookups[2]=3;chains[0]=-4;chains[2]=-7;load_rc=-9;
 if(!setjmp(done))launcher_main();
 assert(attempt_count==3 && calls==2 && partition_result==-9 && boot_info_valid);
 assert(attempts[0].chain==-4 && attempts[1].lookup==-10 && attempts[1].chain==0x7fffffff);
 assert(attempts[2].chain==-7 && attempts[2].first==3 && attempts[2].last==4);
 attempt_count=0; calls=0; success_slot=0;
 int result=setjmp(done); if(!result)launcher_main();
 assert(result==1 && calls==1 && attempt_count==1);
 return 0;
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.c').write_text(pre+s+post)
 subprocess.run(['cc','-std=c11','-fsanitize=address,undefined',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
print('Launcher report: failure codes, skipped chain and successful handoff passed')
