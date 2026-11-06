"""Compile actual status formatter; check bounded output and ring ordering."""
from pathlib import Path
import subprocess,tempfile,json
root=Path(__file__).resolve().parents[2]
s=(root/'src/boards/presto/presto_net.c').read_text()
a=s.index('static struct {',s.index('// Fixed-size, core-0-only'));b=s.index('} nd;',a)+5
fn=s[s.index('void net_status_json('):s.index('// between frames: act on a request')]
head=r'''
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#define WEBDIAG 1
typedef unsigned uint;
enum { F_BUSY,F_ERROR,F_READY,F_IDLE };
static struct {int state;unsigned got,length;char error[80];} fetch={.state=F_IDLE};
static int link_status=3,conn_count=2;
static const char *link_name(void){return "up";}
static void api_json_str(char *o,size_t n,const char *s){snprintf(o,n,"\"%s\"",s);}
static uint32_t time_us_32(void){return 100000;}
static uint64_t time_us_64(void){return 100000;}
typedef struct conn {struct conn *next;unsigned state;uint32_t accepted_us;} conn_t;
static conn_t c2={0,3,90000},c1={&c2,2,80000},*conns=&c1;
'''
main=r'''
int main(void){
 memset(&nd,0xff,sizeof nd);nd.transitions=10;
 for(unsigned i=2;i<10;i++){nd.history[i%8].ms=i*1000;nd.history[i%8].state=i%4;}
 for(unsigned n=400;n<=8192;n+=7792){
  char j[8193];memset(j,0x5a,sizeof j);net_status_json(j,n);
  assert(j[n]==0x5a);assert(strlen(j)<n);puts(j);
 }
}
'''
with tempfile.TemporaryDirectory() as t:
 p=Path(t);(p/'test.c').write_text(head+s[a:b]+'\n'+fn+main)
 subprocess.run(['cc','-Wall','-Wextra','-fsanitize=address,undefined',str(p/'test.c'),'-o',str(p/'test')],check=True)
 lines=subprocess.check_output([str(p/'test')],text=True).splitlines()
 small,big=[json.loads('{'+x[1:]+'}') for x in lines]
 assert 'network' not in small
 n=big['network'];assert n['states']==[0,0,1,1,0] and n['oldest_ms']==20
 assert n['links']==[[i*1000,i%4] for i in range(2,10)]
 print('Network JSON bounds, small caller compatibility, state counts and ring ordering passed')

# Optional integration check: the diagnostic formatter must survive compilation
# in every machine, not only in the host fixture above.
import sys
if len(sys.argv)>1:
 build=Path(sys.argv[1])
 for machine,target in [('master','presto_master'),('beeb','presto_beebjit'),('c64','presto_c64')]:
  assert '-DWEBDIAG=1' in (build/machine/'build.ninja').read_text(),machine
  binary=(build/machine/(target+'.elf')).read_bytes()
  assert b'"network":{"version":1' in binary,machine
  assert b'"poll_gap_us"' in binary,machine
 print('All three diagnostic firmware images contain the network formatter')
