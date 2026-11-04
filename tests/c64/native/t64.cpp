#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <cassert>
#include "t64.h"
extern "C" {
void *psram_malloc(size_t n){return malloc(n);}void psram_free(void *p){free(p);}void *psram_realloc(void *p,size_t n){return realloc(p,n);}
void c64_keyboard_poll(uint8_t *a,uint8_t *b,uint8_t *j){memset(a,255,8);memset(b,255,8);*j=255;}
uint8_t input_get_joystick2(){return 255;}void sid_add_sample(int16_t,int16_t){}int sid_get_buffer_fill(){return 0;}
void c64_init();bool c64_run_frame();uint8_t *c64_get_ram();void c64_reset();
}
static void word(uint8_t *p,unsigned n){p[0]=n;p[1]=n>>8;}
int main(){
    uint8_t image[256]={};memcpy(image,"C64 tape image file",19);
    word(image+32,0x101);word(image+34,2);word(image+36,2);
    // 10 POKE 49152,42:END (second entry writes 99 instead).
    const uint8_t program[]={0x11,8,10,0,0x97,'4','9','1','5','2',',','4','2',':',0x80,0,0,0};
    for(unsigned i=0;i<2;i++){
        uint8_t *p=image+64+i*32;p[0]=1;p[1]=0x82;word(p+2,0x801);word(p+4,0x801+sizeof program);word(p+8,128+i*32);
        memcpy(image+128+i*32,program,sizeof program);
    }
    image[171]='9';image[172]='9';
    t64_archive a;assert(!t64_open(&a,image,sizeof image));
    c64_init();
    for(unsigned selected=0;selected<2;selected++){
        c64_reset();for(int f=0;f<100;f++)c64_run_frame();
        uint8_t *ram=c64_get_ram();ram[49152]=0;
        assert(t64_load(&a,selected,ram));
        for(int f=0;f<100;f++)c64_run_frame();
        assert(ram[49152]==0); // Load alone must not execute.
        const char *typing="RUN\r";
        for(int f=0;f<100;f++){
            c64_run_frame();if(*typing && ram[198]<10){ram[631+ram[198]]=*typing++;ram[198]++;}
        }
        assert(ram[49152]==(selected?99:42));
    }
    puts("Actual C64 core: both T64 entries load after reset, stay idle until RUN, then execute correctly");
}
