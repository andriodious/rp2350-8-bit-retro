#!/usr/bin/env python3
"""Run the actual four-machine launcher with simulated ROM/reset inputs.

This tests software decisions, not RUN-pin, flash, PSRAM or bus timing. No
firmware is built or flashed. Both normal USB fallback and diagnostic reporting
are exercised with address/undefined-behaviour sanitizers.
"""
from pathlib import Path
import json
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
sdk = root / "deps/pico-sdk/src"
source = (root / "src/launcher/launcher.c").read_text()
source = re.sub(r'^#include .*$', '', source, flags=re.M)
start = source.index('static void __attribute__((noreturn)) report_fallback(void) {')
end = source.index('\n#endif', start)
source = source[:start] + '''static void __attribute__((noreturn)) report_fallback(void) {
    longjmp(done, 2);
}''' + source[end:]
source = source.replace('int main(void)', 'int launcher_main(void)')

def size(value):
    return int(value[:-1]) * 1024 if value.endswith('K') else int(value, 0)

partitions = json.loads((root / 'src/launcher/partitions.json').read_text())['partitions']
locations = []
for part in partitions:
    first = size(part['start']) // 4096
    last = (size(part['start']) + size(part['size'])) // 4096 - 1
    locations.append(f'{{{first}u, {last}u}}')

prefix = r'''
#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include <string.h>
#include "system.h"
#include "boot/picobin.h"
#include "boot/bootrom_constants.h"
#define __aligned(x) __attribute__((aligned(x)))
#define FLASH_SECTOR_SIZE 4096u
#define XIP_BASE 0x10000000u
static system_settings_t settings;
#define XIP_NOCACHE_NOALLOC_BASE ((uintptr_t)&settings)
#define BOARD_FLASH_SYSTEM_OFFSET 0
static struct { uint32_t scratch[8]; } hw;
#define watchdog_hw (&hw)
typedef struct { uint32_t boot_word, boot_diagnostic; } boot_info_t;
static jmp_buf done;
static int visited[4], visited_count, chosen, load_result;
static unsigned lookup_mask, boot_mask, reset_reason;
static unsigned cases;
static const unsigned locations[][2] = { LOCATIONS };
static bool watchdog_enable_caused_reboot(void) {
    // Same predicate as the pinned SDK; reset_reason is a mocked register.
    return reset_reason && hw.scratch[4] == 0x6ab73121u;
}
static void startup_diag_mark(unsigned stage) { (void)stage; }
static bool rom_get_boot_info(boot_info_t *b) { memset(b,0,sizeof *b); return true; }
static int rom_load_partition_table(uint8_t *p, unsigned n, bool force) {
    assert(p && n >= 3264 && !force); return load_result;
}
int partition_diag_load(uint8_t *p, uint32_t n) {
    return rom_load_partition_table(p,n,false);
}
static int rom_get_partition_table_info(uint32_t *out, unsigned n, uint32_t flags) {
    unsigned slot = flags >> 24;
    assert(slot < 5 && slot != SYSTEM_SLOT_APP && n == 3);
    assert(visited_count < 4);
    visited[visited_count++] = slot;
    assert((flags & 0xffffffu) == (PT_INFO_PARTITION_LOCATION_AND_FLAGS | PT_INFO_SINGLE_PARTITION));
    if (!(lookup_mask & (1u << slot))) return -1;
    out[0] = flags; out[2] = 0;
    out[1] = (locations[slot][0] << PICOBIN_PARTITION_LOCATION_FIRST_SECTOR_LSB) |
             (locations[slot][1] << PICOBIN_PARTITION_LOCATION_LAST_SECTOR_LSB);
    return 3;
}
static int rom_chain_image(uint8_t *work, unsigned n, uint32_t base, uint32_t bytes) {
    unsigned slot = visited[visited_count-1];
    assert(work && n >= 3264);
    assert(base == XIP_BASE + locations[slot][0] * FLASH_SECTOR_SIZE);
    assert(bytes == (locations[slot][1] + 1 - locations[slot][0]) * FLASH_SECTOR_SIZE);
    if (boot_mask & (1u << slot)) { chosen=slot; longjmp(done,1); }
    return -7;
}
static void __attribute__((noreturn)) reset_usb_boot(unsigned gpio, unsigned disable) {
    assert(!gpio && !disable); longjmp(done,2);
}
'''.replace('LOCATIONS', ','.join(locations))

suffix = r'''
static void run_case(int expected_first) {
    const int slots[] = {SYSTEM_SLOT_MASTER,SYSTEM_SLOT_BEEB,SYSTEM_SLOT_C64,SYSTEM_SLOT_SPECTRUM};
    int expected[4], count=1, final=-1, length=4;
    expected[0]=expected_first;
    for(int i=0;i<4;i++) if(slots[i]!=expected_first) expected[count++]=slots[i];
    for(int i=0;i<4;i++) if((lookup_mask & boot_mask) & (1u<<expected[i])) {
        final=expected[i];length=i+1;break;
    }
    uint32_t saved[8]; memcpy(saved,hw.scratch,sizeof saved);
    visited_count=0;chosen=-1;
#if LAUNCHER_BOOT_REPORT
    attempt_count=0;
#endif
    int result=setjmp(done); if(!result)launcher_main();
    assert(result==(final<0?2:1) && chosen==final && visited_count==length);
    for(int i=0;i<length;i++) assert(visited[i]==expected[i]);
    for(int i=1;i<8;i++) assert(hw.scratch[i]==saved[i]);
    bool fallback=reset_reason && saved[4]==0x6ab73121u &&
                  (saved[0]&0xffff0000u)==C64_DIAG_MAGIC;
    assert(hw.scratch[0]==(fallback ? C64_RETURN_MAGIC|(saved[0]&511u) : saved[0]));
#if LAUNCHER_BOOT_REPORT
    assert(attempt_count==(unsigned)length && partition_result==load_result && boot_info_valid);
    for(int i=0;i<length;i++) {
        assert(attempts[i].slot==expected[i]);
        assert(attempts[i].lookup==((lookup_mask & (1u<<expected[i]))?3:-1));
    }
#endif
    cases++;
}
int main(void) {
    const char *names[]={"master","beeb","c64","spectrum"};
    for(unsigned machine=0;machine<4;machine++)
    for(unsigned reason=0;reason<3;reason++)
    for(unsigned armed=0;armed<2;armed++)
    for(unsigned magic=0;magic<2;magic++)
    for(unsigned lookup=0;lookup<32;lookup++)
    for(unsigned boot=0;boot<32;boot++) {
        memset(&settings,0,sizeof settings); settings.magic=SYSTEM_MAGIC; settings.version=SYSTEM_VERSION;
        strcpy(settings.machine,names[machine]);
        for(unsigned i=0;i<8;i++) hw.scratch[i]=0x12345678u+i;
        hw.scratch[0]=(magic?C64_DIAG_MAGIC:0x98760000u)|0x106u;
        hw.scratch[4]=armed?0x6ab73121u:0;
        reset_reason=reason;lookup_mask=lookup;boot_mask=boot;
        load_result=(boot&1)?-9:0;
        run_case(reason&&armed&&magic ? SYSTEM_SLOT_MASTER : system_slot(names[machine]));
    }
    // Missing, damaged, unterminated and unsupported saved choices default safely.
    reset_reason=0;lookup_mask=boot_mask=31;
    memset(&settings,0xff,sizeof settings);run_case(SYSTEM_SLOT_MASTER);
    settings.magic=SYSTEM_MAGIC;settings.version=SYSTEM_VERSION;run_case(SYSTEM_SLOT_MASTER);
    strcpy(settings.machine,"unknown");run_case(SYSTEM_SLOT_MASTER);
    strcpy(settings.machine,"c64");settings.version++;run_case(SYSTEM_SLOT_MASTER);
    // Deterministic malformed records exercise the bounded string validation.
    uint32_t random=1;
    for(unsigned trial=0;trial<100000;trial++) {
        unsigned char *p=(unsigned char*)&settings;
        for(unsigned j=0;j<sizeof settings;j++){random=random*1664525u+1013904223u;p[j]=random>>24;}
        if(trial&1){settings.magic=SYSTEM_MAGIC;settings.version=SYSTEM_VERSION;}
        assert(!strcmp(system_settings_machine(&settings),SYSTEM_DEFAULT_MACHINE));
    }
    printf("launcher report=%d: %u reset/settings/fallback cases and 100000 malformed records passed\n",
           LAUNCHER_BOOT_REPORT,cases);
    return 0;
}
'''

with tempfile.TemporaryDirectory(prefix='presto-launcher-') as directory:
    path = Path(directory)
    (path/'test.c').write_text(prefix+source+suffix)
    for report in (0,1):
        executable = path/f'test-{report}'
        subprocess.run([
            'cc','-std=c11','-g','-fsanitize=address,undefined','-fno-sanitize-recover=all',
            '-DNO_PICO_PLATFORM=1',f'-DLAUNCHER_BOOT_REPORT={report}',f'-DSTARTUP_DIAG={report}',
            '-I'+str(root/'src/common'),
            '-I'+str(sdk/'common/boot_picobin_headers/include'),
            '-I'+str(sdk/'rp2_common/boot_bootrom_headers/include'),
            str(path/'test.c'),'-o',str(executable)],check=True)
        subprocess.run([str(executable)],check=True)
