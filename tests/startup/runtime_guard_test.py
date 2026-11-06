"""Verify per-core diagnostic hooks do not change LEDs during core-1 startup."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[2]
s='\n'.join(x for x in (root/'src/boards/presto/startup_runtime_diag.c').read_text().splitlines() if not x.startswith('#include'))
stub='''
#include <assert.h>
#define PICO_RUNTIME_INIT_FUNC_PER_CORE(f,p)
static unsigned core, count, last;
unsigned get_core_num(void){return core;}
void startup_diag_mark(unsigned stage){count++;last=stage;}
int __real_main(void){return 17;}
'''
main='''
int main(void){
 core=1;startup_runtime_ready();assert(count==0);
 core=0;startup_runtime_ready();assert(count==1);
 assert(last==(STARTUP_DIAG_LAUNCHER?0x402:0x405));
 assert(__wrap_main()==17);assert(count==2);
 assert(last==(STARTUP_DIAG_LAUNCHER?0x404:0x407));
}
'''
with tempfile.TemporaryDirectory() as t:
 p=Path(t);(p/'test.c').write_text(stub+s+main)
 for launcher in (0,1):
  subprocess.run(['cc','-fsanitize=address,undefined',f'-DSTARTUP_DIAG_LAUNCHER={launcher}',str(p/'test.c'),'-o',str(p/'test')],check=True)
  subprocess.run([str(p/'test')],check=True)
print('Core-1 guard and launcher/machine markers passed')
