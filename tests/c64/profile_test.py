"""Test profile windows, 32-bit timer wrap and disabled call semantics on host."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory() as td:
    p=Path(td);(p/'pico').mkdir()
    (p/'pico/stdlib.h').write_text('#include <stdint.h>\ninline uint32_t tick=0; inline uint32_t time_us_32(){return tick++;}\n')
    (p/'test.cpp').write_text(r'''
#include "profile.h"
#include <cassert>
#include <cstring>
int main(){
 int calls=0;
 C64_PROFILE_CALL(CPU, ++calls);
 assert(calls==1);
#if C64_PROFILE
 using namespace c64_profile;
 reset();begin(100,false);
 tick=0xffffffffu;C64_PROFILE_CALL(CPU, ++calls);
 assert(active.us[CPU]==1);
 add(CONVERSION,700);
 assert(!finish(4999999,true));
 assert(finish(5000100,false));
 assert(last.frames==2 && last.rendered==1 && last.elapsed==5000000);
 assert(last.us[CONVERSION]==700 && last.us[CPU]==1);
 begin(5000200,true);assert(active.frames==0 && active.turbo);
 assert(!finish(5000300,true));
 reset();begin(6000000,false);assert(active.us[CONVERSION]==0);
 assert(last.frames==2); // USB returns the last complete window.
 char small[8];json(small,sizeof small);assert(small[0]==0);
 char full[1024];json(full,sizeof full);assert(strstr(full,"\"sequence\":1"));
 assert(strstr(full,"\"frames\":2"));
 print();
#endif
}
''')
    for enabled in (0,1):
        subprocess.run(['c++','-std=c++20','-fsanitize=address,undefined',f'-DC64_PROFILE={enabled}','-I'+str(p),'-I'+str(root/'src/machines/c64'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
        subprocess.run([str(p/'test')],check=True)
print('Profile windows, mode reset, timer wrap and disabled-call checks passed')
