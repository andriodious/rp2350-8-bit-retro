// Exercise the actual parser/loader under ASan/UBSan, including malformed images.
#include "t64.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
static uint8_t image[4096], ram[65536];
static void word(unsigned p,unsigned n){image[p]=n;image[p+1]=n>>8;}
static unsigned fixture(unsigned slots,unsigned count){
    memset(image,0,sizeof image);memcpy(image,"C64 tape image file",19);
    word(32,0x101);word(34,slots);word(36,count);
    for(unsigned i=0;i<count;i++){
        unsigned p=64+i*32,offset=64+slots*32+i*16;
        image[p]=1;image[p+1]=0x82;word(p+2,0x801);word(p+4,0x80c);word(p+8,offset);
        memcpy(image+p+16,"TEST            ",16);
        const uint8_t basic[]={0x0a,0x08,10,0,0x99,'"','O','K','"',0,0,0};
        // next line is at $080b; 12-byte program ends at $080d.
        memcpy(image+offset,basic,sizeof basic);image[offset]=0x0b;word(p+4,0x80d);
    }
    return 64+slots*32+count*16;
}
int main(void){
    struct t64_archive a;struct t64_entry e;
    unsigned n=fixture(3,2);
    assert(!t64_open(&a,image,n)&&a.count==2);
    assert(t64_entry_get(&a,0,&e)&&e.runnable&&!strcmp(e.name,"TEST"));
    memset(ram,0x55,sizeof ram);assert(t64_load(&a,0,ram));
    assert(!memcmp(ram+0x801,image+160,12));assert(ram[0x800]==0x55 && ram[0x80d]==0x55);
    assert(ram[0x2d]==0x0d && ram[0x2e]==8 && ram[0x31]==0x0d);
    assert(!t64_load(&a,2,ram));
    // A raw machine-code entry never offers RUN.
    word(66,0xc000);word(68,0xc00c);
    assert(!t64_open(&a,image,n));assert(t64_entry_get(&a,0,&e)&&!e.runnable);
    // Every truncated prefix fails safely.
    n=fixture(3,2);for(unsigned i=0;i<188;i++)assert(t64_open(&a,image,i));
    image[0]='X';assert(t64_open(&a,image,n));n=fixture(3,2);
    word(34,65535);assert(t64_open(&a,image,n));n=fixture(3,2);
    word(68,0x800);assert(t64_open(&a,image,n));n=fixture(3,2);
    memset(image+72,0xff,4);assert(t64_open(&a,image,n));n=fixture(3,2);
    word(72,65);assert(t64_open(&a,image,n));n=fixture(3,2);
    word(104,160);assert(t64_open(&a,image,n));n=fixture(3,2);
    image[65]=0x81;assert(t64_open(&a,image,n));n=fixture(3,2);
    image[64]=3;assert(t64_open(&a,image,n));n=fixture(3,2);
    // Stale used count and unused directory slots are harmless.
    word(36,0);assert(!t64_open(&a,image,n)&&a.count==2);
    // Bad BASIC links: loadable, but do not auto-RUN.
    image[160]=0xff;assert(!t64_open(&a,image,n));assert(t64_entry_get(&a,0,&e)&&!e.runnable);
    n=fixture(65,65);assert(t64_open(&a,image,n));
    n=fixture(64,64);assert(!t64_open(&a,image,n)&&a.count==64);
    // Exclusive end address $10000 is encoded as zero.
    n=fixture(1,1);word(66,0xfff4);word(68,0);assert(!t64_open(&a,image,n));
    assert(t64_load(&a,0,ram)&&ram[65535]==0);
    // Mutation corpus exercises directory/address arithmetic and BASIC links.
    srand(7);
    for(unsigned i=0;i<30000;i++){
        n=fixture(3,2);unsigned pos=(unsigned)rand()%n;image[pos]=(uint8_t)rand();
        if(!t64_open(&a,image,n))for(unsigned j=0;j<a.count;j++)assert(t64_load(&a,j,ram));
    }
    puts("T64 parser/load bounds, BASIC detection, limits and mutation corpus passed");
}
