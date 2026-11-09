"""Exercise real diagnostic handler/setter with mocked watchdog hardware."""
from pathlib import Path
import subprocess, tempfile
root=Path(__file__).resolve().parents[2]
main=(root/'src/machines/c64/main.cpp').read_text()
api=(root/'src/common/api/api.c').read_text()
setter=main.split('extern "C" bool c64_watchdog_set',1)[1].split('\n#endif',1)[0]
handler=api.split('static void cmd_c64_watchdog',1)[1].split('\n#endif',1)[0]
code=r'''
#include <cassert>
#include <cstring>
#include <cstdio>
#include <cstdarg>
static bool recovery=false, fault_seen=false, enabled=true;
static int changes=0;
void watchdog_enable(unsigned ms,bool debug){assert(ms==8000 && debug);enabled=true;changes++;}
void watchdog_disable(){enabled=false;changes++;}
struct api_req_t {const char *args;int code;char out[160];};
void api_reply(api_req_t *r,int status,const char *format,...){r->code=status;va_list a;va_start(a,format);vsnprintf(r->out,sizeof r->out,format,a);va_end(a);}
'''+ 'bool c64_watchdog_set'+setter+'\nvoid cmd_c64_watchdog'+handler+r'''
int main(){
 api_req_t r={"on=0",0,{0}};cmd_c64_watchdog(&r);assert(!enabled && r.code==200 && changes==1);
 r.args="on=1";cmd_c64_watchdog(&r);assert(enabled && r.code==200 && changes==2);
 for(const char *bad:{"","on=2","on=10","on=0&x=1"}){r.args=bad;cmd_c64_watchdog(&r);assert(r.code==400 && changes==2);}
 r.args="on=0";recovery=true;cmd_c64_watchdog(&r);assert(r.code==409 && enabled && changes==2);
 recovery=false;fault_seen=true;cmd_c64_watchdog(&r);assert(r.code==409 && enabled && changes==2);
}
'''
code='#include <initializer_list>\n'+code
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'test.cpp';p.write_text(code);exe=Path(d)/'test'
 subprocess.run(['c++','-std=c++17','-fsanitize=address,undefined',str(p),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
print('Watchdog toggle: enable/disable, strict input and recovery/fault guards passed')
