#!/usr/bin/env python3
"""Execute terminal capture + actual Master JSON decoder with mocked scratch HW."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory() as t:
 p=Path(t)
 for f in ['pico/stdlib.h','hardware/sync.h','hardware/watchdog.h','hardware/structs/watchdog.h']:
  q=p/f;q.parent.mkdir(parents=True,exist_ok=True);q.write_text('#include "stub.h"\n')
 (p/'stub.h').write_text('''#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <setjmp.h>
extern jmp_buf jump;
static struct {uint32_t scratch[8];} hw;
#define watchdog_hw (&hw)
static unsigned core,enabled,marked;
static unsigned get_core_num(void){return core;}
static unsigned save_and_disable_interrupts(void){return 0;}
static void __dmb(void){}
static void watchdog_enable(unsigned ms,bool pause){enabled=ms;(void)pause;}
static void __attribute__((noreturn)) __wfi(void){longjmp(jump,1);}
''')
 source=(root/'src/machines/c64/abort_diag.c').read_text().replace(' _exit(', ' diag_exit(')
 decoder=(root/'src/common/system.c').read_text().split('static bool c64_failed;')[1]
 (p/'test.c').write_text('''#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "stub.h"
jmp_buf jump;
void c64_terminal_mark(void){marked++;}
'''+source+'\nstatic bool c64_failed;'+decoder+'''
int main(void){
 for(unsigned kind=1;kind<=6;kind++){
  memset(&hw,0,sizeof hw);core=kind%2;hw.scratch[0]=C64_DIAG_MAGIC|6;
  for(unsigned i=4;i<8;i++)hw.scratch[i]=0x12340000+i;
  c64_abort_stage(7);enabled=marked=0;
  if(!setjmp(jump))switch(kind){
   case 1:__wrap_abort();case 2:__wrap_exit(42);case 3:diag_exit(43);
   case 4:__assert_func("file.c",321,"func","expression");
   case 5:__wrap_panic("panic %u",123);case 6:hard_assertion_failure();
  }
  assert(enabled==1000&&marked==1);
  assert(c64_abort_valid(hw.scratch[1]));assert(((hw.scratch[1]>>20)&15)==kind);
  assert(((hw.scratch[1]>>16)&15)==7);assert(hw.scratch[2]);
  if(kind==4)assert((hw.scratch[1]&65535)==321);
  if(kind==2||kind==3)assert(hw.scratch[3]==40+kind);
  for(unsigned i=4;i<8;i++)assert(hw.scratch[i]==0x12340000+i);
  hw.scratch[0]=C64_RETURN_MAGIC|(hw.scratch[0]&511);
  system_capture_c64_failure();char out[512];system_failure_json(out,sizeof out);
  assert(strstr(out,"terminal-call")&&strstr(out,"frontend_stage"));puts(out);
 }
 hw.scratch[0]=C64_RETURN_MAGIC|6;hw.scratch[1]=0x8200;hw.scratch[2]=0x200001d4;
 system_capture_c64_failure();char out[512];system_failure_json(out,sizeof out);
 assert(strstr(out,"watchdog")&&strstr(out,"00008200"));
 puts("Terminal hooks, core/stage retention, scratch isolation and legacy fault decode passed");
}
''')
 subprocess.run(['cc','-DC64_ABORT_DIAG=1','-fsanitize=address,undefined','-g','-I'+str(p),'-I'+str(root/'src/common'),str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
