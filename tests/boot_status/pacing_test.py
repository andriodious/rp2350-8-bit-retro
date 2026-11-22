#!/usr/bin/env python3
"""Exercise the real paused frontend loop and Master's resume adjustment."""
from pathlib import Path
import subprocess, tempfile
root=Path(__file__).resolve().parents[2]
front=(root/'src/common/frontend.c').read_text()
front=front[front.index('static uint64_t s_menu_paused_us;'):]
master=(root/'src/machines/master/main.c').read_text()
start=master.index('            uint64_t paused_before = frontend_paused_us();')
end=master.index('            frame_hook(m);',start)
resume=master[start:end]
with tempfile.TemporaryDirectory() as directory:
    p=Path(directory)
    source=r'''
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
static uint64_t now;
static unsigned menu_steps,polls,watchdogs;
static uint64_t time_us_64(void){return now;}
static void frontend_frame_once(void){now+=100;polls++;if(menu_steps)menu_steps--;}
static bool boot_status_visible(void){return menu_steps!=0;}
static void watchdog_update(void){watchdogs++;}
static void sleep_ms(unsigned ms){now+=(uint64_t)ms*1000;}
'''+front+r'''
static void presto_machine_frame(void){frontend_frame();}
static void run_frame(uint64_t *epoch,uint64_t *stats){
 uint64_t t0=*epoch,stat_t=*stats;
'''+resume+r'''
 *epoch=t0;*stats=stat_t;
}
int main(void){
 uint64_t epoch=0,stats=0;now=1000000;
 run_frame(&epoch,&stats);assert(epoch==0&&frontend_paused_us()==0);
 uint64_t before=now;unsigned old_polls=polls;
 menu_steps=501;run_frame(&epoch,&stats);
 assert(epoch==now-before&&stats==epoch);
 assert(polls-old_polls==501&&watchdogs==500);
 // The CPU owes only the pre-menu elapsed cycles, not ten seconds of catch-up.
 assert((now-epoch)*2==before*2);
 uint64_t prior=epoch;before=now;
 menu_steps=101;run_frame(&epoch,&stats);
 assert(epoch-prior==now-before&&stats==epoch);
 before=epoch;run_frame(&epoch,&stats);assert(epoch==before);
 // Reset rebases from now on the next main-loop iteration, as before.
 epoch=now;assert((now-epoch)*2==0);
}
'''
    (p/'test.c').write_text(source)
    subprocess.run(['cc','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(p/'test.c'),'-o',str(p/'test')],check=True)
    subprocess.run([str(p/'test')],check=True)
print('Paused frontend: network/input polling retained; Master has no post-menu catch-up debt: PASS')
