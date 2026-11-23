"""Run the actual flash-write callback with an SDK that resets XIP read mode."""
from pathlib import Path
import subprocess, tempfile
root=Path(__file__).resolve().parents[2]
s=(root/'src/common/mem.c').read_text()
a=s.index('static void __no_inline_not_in_flash_func(write_sector)')
b=s.index('static uint32_t s_writes;',a)
callback=s[a:b]
stub=r'''
#include <assert.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#define __no_inline_not_in_flash_func(x) x
#define FLASH_SECTOR_SIZE 4096u
#define FLASH_PAGE_SIZE 256u
struct window {uint32_t timing,rcmd,rfmt,wcmd,wfmt;};
static struct {struct window m[2];} qmi;
#define qmi_hw (&qmi)
static struct {uint32_t offset,len;const uint8_t *data;} s_w;
static uint8_t s_tail[FLASH_PAGE_SIZE],data[4096],flash[4096];
static unsigned barriers;
static void __dsb(void){barriers++;}
static void __isb(void){barriers++;}
static void downgrade(void){qmi.m[0].timing=0x40000004;qmi.m[0].rcmd=3;qmi.m[0].rfmt=4096;}
static void flash_range_erase(uint32_t off,size_t len){assert(off==0&&len==4096);memset(flash,255,sizeof flash);downgrade();}
static void flash_range_program(uint32_t off,const uint8_t *p,size_t len){assert(off+len<=4096&&len%256==0);memcpy(flash+off,p,len);downgrade();}
'''
main=r'''
int main(void){
 for(unsigned i=0;i<4096;i++)data[i]=(uint8_t)(i*31);
 for(unsigned mode=0;mode<2;mode++)for(unsigned len=0;len<=4096;len++){
  struct window initial={0x60007203,mode?235:3,mode?299688:4096,40962,4096};
  struct window psram={1614693122,235,397994,56,4778};
  qmi.m[0]=initial;qmi.m[1]=psram;s_w.len=len;s_w.data=data;barriers=0;
  write_sector(NULL);
  assert(!memcmp(&qmi.m[0],&initial,sizeof initial));
  assert(!memcmp(&qmi.m[1],&psram,sizeof psram));
  assert(!memcmp(data,flash,len));for(unsigned i=len;i<4096;i++)assert(flash[i]==255);
  assert(barriers==3);
 }
 puts("Flash protocol/timing, PSRAM untouched, every sector length: PASS");
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.c').write_text(stub+callback+main)
 subprocess.run(['cc','-std=c11','-O1','-fsanitize=address,undefined',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
