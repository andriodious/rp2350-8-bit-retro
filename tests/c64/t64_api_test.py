"""Compile the production T64 API handlers and exercise bounded replies/commands."""
from pathlib import Path
import json, subprocess, tempfile
root = Path(__file__).resolve().parents[2]
source = (root/'src/common/api/api.c').read_text()
handlers = source[source.index('static void cmd_tape(api_req_t'):source.index('// ---- The commands')].rsplit('#endif', 1)[0]
escape = source[source.index('int api_json_str('):source.index('// "1" on')]
fixture = r'''
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <assert.h>
#include "t64.h"
typedef struct {char *out; size_t size; const char *args; int code; bool json;} api_req_t;
unsigned c64_tape_count(void){return 64;}
uint32_t c64_tape_generation(void){return 7;}
bool c64_tape_entry(unsigned index,struct t64_entry *e){
 memset(e,0,sizeof *e);memset(e->name,'"',16);e->start=65535;e->length=65536;e->runnable=true;return index<64;
}
bool c64_tape_select(unsigned index,bool run){return index<64 && (!run || index==0);}
void api_error(api_req_t *r,int code,const char *why){r->code=code;snprintf(r->out,r->size,"{\"error\":\"%s\"}",why);}
void api_reply(api_req_t *r,int code,const char *fmt,...){r->code=code;va_list ap;va_start(ap,fmt);vsnprintf(r->out,r->size,fmt,ap);va_end(ap);}
'''
main = r'''
int main(void){
 char reply[8192]; api_req_t r={reply,sizeof reply,"",200,true};
 cmd_tape(&r);assert(r.code==200);puts(reply);
 // Every short response capacity must stay within its allocation.
 for(unsigned size=1;size<=8192;size+=17){
   char *small=malloc(size);r=(api_req_t){small,size,"",200,true};cmd_tape(&r);free(small);
 }
 r=(api_req_t){reply,sizeof reply,"7 0 1",200,true};cmd_tape_load(&r);assert(r.code==200);
 r.args="6 0 1";cmd_tape_load(&r);assert(r.code==409);
 r.args="7 64 0";cmd_tape_load(&r);assert(r.code==400);
 r.args="7 1 1";cmd_tape_load(&r);assert(r.code==400);
 r.args="7 0 2";cmd_tape_load(&r);assert(r.code==400);
 r.args="7 0 1 garbage";cmd_tape_load(&r);assert(r.code==400);
 r.args="7";cmd_tape_load(&r);assert(r.code==400);
}
'''
with tempfile.TemporaryDirectory() as td:
    src=Path(td)/'test.c';exe=Path(td)/'test'
    src.write_text(fixture+escape+handlers+main)
    subprocess.run(['cc','-std=c11','-fsanitize=address,undefined','-I'+str(root/'src/machines/c64'),str(src),'-o',str(exe)],check=True)
    result=subprocess.run([str(exe)],capture_output=True,text=True,check=True)
    data=json.loads(result.stdout)
    assert len(data['entries'])==64 and data['entries'][63]['name']=='"'*16
print('Actual T64 API handlers: 64 escaped names, bounded responses, stale generation and invalid commands passed')
