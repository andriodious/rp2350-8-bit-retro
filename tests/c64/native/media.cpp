#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <cassert>
#include <vector>
#include "sysdeps.h"
#include "C64.h"
#include "Cartridge.h"
#include "t64.h"
#include "crt.h"
#undef FILE
#undef fopen
#undef fclose
#undef fread
#undef fseek
#undef ftell
#undef rewind
extern C64 *TheC64;
static int fail_alloc=-1;
extern "C" {
void *psram_malloc(size_t n){if(fail_alloc==0)return nullptr;if(fail_alloc>0)--fail_alloc;return malloc(n);}
void psram_free(void *p){free(p);}void *psram_realloc(void *p,size_t n){return realloc(p,n);}
void c64_keyboard_poll(uint8_t *a,uint8_t *b,uint8_t *j){memset(a,255,8);memset(b,255,8);*j=255;}
uint8_t input_get_joystick2(){return 255;}void sid_add_sample(int16_t,int16_t){}int sid_get_buffer_fill(){return 0;}
void c64_init();bool c64_run_frame();uint8_t *c64_get_ram();void c64_reset();
}
static void word(uint8_t *p,unsigned n){p[0]=n>>8;p[1]=n;}
static void dword(uint8_t *p,unsigned n){word(p,n>>16);word(p+2,n);}
static std::vector<uint8_t> cart(){
 std::vector<uint8_t>b(64);memcpy(b.data(),"C64 CARTRIDGE   ",16);dword(b.data()+16,64);word(b.data()+20,256);word(b.data()+22,32);b[24]=1;
 for(unsigned bank=0;bank<2;bank++)for(unsigned high=0;high<2;high++){
  size_t p=b.size();b.resize(p+8208);memcpy(b.data()+p,"CHIP",4);dword(b.data()+p+4,8208);word(b.data()+p+8,2);word(b.data()+p+10,bank);word(b.data()+p+12,high?0xa000:0x8000);word(b.data()+p+14,8192);memset(b.data()+p+16,bank?0x99:0xea,8192);
  if(!bank && high){const uint8_t code[]={0xa9,42,0x8d,0,0xc0,0x4c,5,0xe0};memcpy(b.data()+p+16,code,8);b[p+16+0x1ffc]=0;b[p+16+0x1ffd]=0xe0;}
 }return b;
}
int main(int argc,char **argv){
 c64_init();auto b=cart();assert(!c64_crt_prepare(b.data(),b.size()));auto old=TheC64->TheCart;
 for(int failure=0;failure<2;failure++){fail_alloc=failure;assert(c64_crt_prepare(b.data(),b.size()));assert(TheC64->TheCart==old);}fail_alloc=-1;
 for(unsigned size=0;size<b.size();size+=137){assert(c64_crt_prepare(b.data(),size));assert(TheC64->TheCart==old);}
 for(unsigned offset:{22u,24u,64u,72u,74u,76u,78u}){auto corrupt=b;corrupt[offset]=255;assert(c64_crt_prepare(corrupt.data(),corrupt.size()));assert(TheC64->TheCart==old);}
 auto bad=b;bad[64+4]=255;assert(c64_crt_prepare(bad.data(),bad.size()));assert(TheC64->TheCart==old);
 old->WriteIO1(0,1);assert(old->ReadROML(0,0,true)==0x99);old->WriteIO1(2,4);assert(old->ReadROML(0,0x55,true)==0x55);
 old->WriteIO2(0x12,0x42);assert(old->ReadIO2(0x12,0)==0x42);
 c64_reset();for(int i=0;i<5;i++)c64_run_frame();assert(c64_get_ram()[0xc000]==42);
 for(int i=0;i<20;i++){assert(!c64_crt_prepare(b.data(),b.size()));c64_crt_detach();assert(!c64_crt_active());}
 // Standard 8K and 16K packets, including two separate CHIP packets.
 auto normal=b;normal.resize(64+2*8208);word(normal.data()+22,0);normal[24]=0;word(normal.data()+72,0);word(normal.data()+64+8208+8,0);
 assert(!c64_crt_prepare(normal.data(),normal.size()));assert(!TheC64->TheCart->notEXROM&&!TheC64->TheCart->notGAME);
 normal.resize(64+8208);normal[25]=1;assert(!c64_crt_prepare(normal.data(),normal.size()));assert(TheC64->TheCart->notGAME);c64_crt_detach();
 c64_reset();for(int i=0;i<100;i++)c64_run_frame();
 const uint8_t program[]={0x11,8,10,0,0x97,'4','9','1','5','2',',','4','2',':',0x80,0,0,0};
 std::vector<uint8_t>p(28+sizeof program);memcpy(p.data(),"C64File",7);p[26]=1;p[27]=8;memcpy(p.data()+28,program,sizeof program);
 t64_archive a;assert(!t64_open(&a,p.data(),p.size()));t64_entry e;assert(t64_entry_get(&a,0,&e)&&e.runnable);uint8_t *ram=c64_get_ram();ram[0xc000]=0;assert(t64_load(&a,0,ram));
 const char *typing="RUN\r";for(int i=0;i<150;i++){c64_run_frame();if(*typing&&ram[198]<10){ram[631+ram[198]]=*typing++;ram[198]++;}}assert(ram[0xc000]==42);
 p[26]=255;p[27]=255;assert(t64_open(&a,p.data(),p.size()));
 if(argc==3){
  auto read=[](const char *path){FILE*f=fopen(path,"rb");assert(f);fseek(f,0,SEEK_END);long n=ftell(f);rewind(f);std::vector<uint8_t>b(n);assert(fread(b.data(),1,n,f)==(size_t)n);fclose(f);return b;};
  auto p00=read(argv[1]);assert(!t64_open(&a,p00.data(),p00.size()));assert(t64_entry_get(&a,0,&e));printf("P00 sample: start=%04x length=%u runnable=%d\n",e.start,e.length,e.runnable);
  c64_reset();for(int i=0;i<100;i++)c64_run_frame();assert(t64_load(&a,0,c64_get_ram()));
  const char *run="RUN\r";for(int i=0;i<1000;i++){c64_run_frame();auto r=c64_get_ram();if(*run&&r[198]<10){r[631+r[198]]=*run++;r[198]++;}}puts("P00 sample ran 1000 native frames");
  auto crt=read(argv[2]);assert(!c64_crt_prepare(crt.data(),crt.size()));c64_reset();for(int i=0;i<1000;i++)c64_run_frame();c64_crt_detach();puts("EasyFlash sample ran 1000 native frames");
 }
 puts("CRT bounds, failure preservation, allocation failure, EasyFlash banking/reset boot, repeated eject and actual P00 BASIC execution passed");
}
