// SPDX-License-Identifier: GPL-3.0-or-later
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <vector>
#include <string>
#include <cassert>
#include "drive_bridge.h"
extern "C" {
void *psram_malloc(size_t n){return malloc(n);}void psram_free(void *p){free(p);}void *psram_realloc(void *p,size_t n){return realloc(p,n);}
static uint64_t net_service_calls;
void retro_drive_network_poll(){assert(retro_drive_active());(void)retro_drive_pc();net_service_calls++;}
void c64_keyboard_poll(uint8_t *a,uint8_t *b,uint8_t *j){memset(a,255,8);memset(b,255,8);*j=255;}
uint8_t input_get_joystick2(){return 255;}void sid_add_sample(int16_t,int16_t){}int sid_get_buffer_fill(){return 0;}
void c64_init();bool c64_run_frame();uint8_t *c64_get_ram();uint8_t *c64_get_framebuffer();
}
static std::vector<uint8_t> read(const char *p){FILE *f=fopen(p,"rb");assert(f);fseek(f,0,SEEK_END);long n=ftell(f);assert(n>0);rewind(f);std::vector<uint8_t>b(n);assert(fread(b.data(),1,n,f)==size_t(n));fclose(f);return b;}
static void frames(int n){while(n--)c64_run_frame();}
static void type(const std::string &s){auto r=c64_get_ram();for(uint8_t c:s){int timeout=500;while(r[198]>=10 && --timeout)frames(1);assert(timeout);r[631+r[198]++]=c;} }
int main(int argc,char **argv){
 assert(argc==5);auto rom=read(argv[1]),disk=read(argv[2]);assert(retro_drive_init(rom.data(),rom.size(),disk.data(),disk.size()));c64_init();frames(1000);
 if(!strcmp(argv[4],"probe")){
  type("10 OPEN15,8,15:FORI=0TO5:READA:PRINT#15,\"M-W\"CHR$(I)CHR$(5)CHR$(1)CHR$(A):NEXT\r");frames(50);
  type("20 PRINT#15,\"U3\":CLOSE15\r");frames(50);
  type("30 DATA169,90,141,0,7,96\r");frames(50);type("RUN\r");frames(1000);
  printf("U3 marker=%02x drive_pc=%04x\n",retro_drive_peek(0x700),retro_drive_pc());
  assert(retro_drive_peek(0x700)==0x5a);puts("PASS Frodo -> IEC -> 1581 M-W/U3 execution");return 0;
 }
 type("LOAD\"*\",8,1\rRUN\r");
 for(int i=1;i<=10000;i++){frames(1);if(i%500==0){
  std::string p=std::string(argv[3])+"/frame-"+std::to_string(i)+".raw";FILE *f=fopen(p.c_str(),"wb");assert(f);fwrite(c64_get_framebuffer(),1,384*272,f);fclose(f);
  // Capture uploaded drive code for loop analysis; test output stays outside the repository.
  std::string dp=std::string(argv[3])+"/drive-"+std::to_string(i)+".bin";
  FILE *df=fopen(dp.c_str(),"wb");assert(df);
  for(unsigned addr=0;addr<8192;addr++){uint8_t v=retro_drive_peek(addr);assert(fwrite(&v,1,1,df)==1);}fclose(df);
  printf("frame=%d drive_pc=%04x\n",i,retro_drive_pc());fflush(stdout);
 }}
}
