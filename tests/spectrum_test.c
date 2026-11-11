#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>
#include "../src/machines/spectrum/spectrum.c"
static unsigned samples;
void spectrum_audio(const float *p,int n){for(int i=0;i<n;i++)assert(isfinite(p[i]));samples+=n;}
static uint8_t external[262144],image[49179];
static void render(void){for(unsigned y=0;y<256;y++)spectrum_render_line(y,zx.fb+y*512);}
static uint32_t hash(void){render();uint32_t h=2166136261u;for(unsigned i=0;i<131072;i++)h=(h^zx.fb[i])*16777619;return h;}
static void shot(const char *name){render();FILE *f=fopen(name,"wb");assert(f);fprintf(f,"P6\n320 256\n255\n");for(int y=0;y<256;y++)for(int x=0;x<320;x++){unsigned p=zx.fb[y*512+x],v=(p&8)?255:205;unsigned char rgb[]={p&2?v:0,p&4?v:0,p&1?v:0};fwrite(rgb,1,3,f);}fclose(f);}
int main(int argc,char **argv){
 assert(argc==2);FILE *f=fopen(argv[1],"rb");assert(f);assert(fread(image,1,sizeof image,f)==sizeof image);fclose(f);
 spectrum_init(external);
 for(int i=0;i<1000;i++)spectrum_exec(1000);
 shot("/tmp/spectrum-basic.ppm");
 assert(spectrum_load_sna(image,sizeof image));assert(spectrum_pc()==0xd463);assert(zx.cpu.sp==0xffef);
 uint16_t pc=zx.cpu.pc;assert(!spectrum_load_sna(image,49178));assert(zx.cpu.pc==pc);
 uint8_t bad[49179];memcpy(bad,image,sizeof bad);bad[23]=0;bad[24]=0;assert(!spectrum_load_sna(bad,sizeof bad));
 memcpy(bad,image,sizeof bad);bad[25]=3;assert(!spectrum_load_sna(bad,sizeof bad));
 for(unsigned k=0;k<40;k++){spectrum_key(k,true);assert(zx.physical_keys[k/5]&(1u<<(k%5)));spectrum_key(k,false);assert(!zx.physical_keys[k/5]);}
 spectrum_key(40,true);spectrum_key(255,true);
 for(int i=0;i<5000;i++)spectrum_exec(1000);
 uint32_t first=hash();shot("/tmp/spectrum-elite.ppm");
 spectrum_load_sna(image,sizeof image);for(int i=0;i<5000;i++)spectrum_exec(1000);
 assert(first==hash());
 assert(first==0x3737ea77u); // Elite IM2 acknowledgement regression: rotating Cobra, not ROM error screen.
 spectrum_key(38,true);for(int i=0;i<2000;i++)spectrum_exec(1000);spectrum_key(38,false);
 for(int i=0;i<2000;i++)spectrum_exec(1000);
 spectrum_key(35,true);for(int i=0;i<1000;i++)spectrum_exec(1000);spectrum_key(35,false);
 for(int i=0;i<3000;i++)spectrum_exec(1000);
 shot("/tmp/spectrum-commander.ppm");
 assert(hash()!=first);
 // Every keyboard half-row selected by Z80 port reads, including combined rows.
 spectrum_key(10,true);spectrum_key(16,true);
 zx.cpu.pc=0x8000;zx.ram[1][0]=0xdb;zx.ram[1][1]=0xfe;zx.ram[1][2]=0x76;zx.cpu.a=0xf3;
 zx.pins=z80_prefetch(&zx.cpu,0x8000);spectrum_exec(20);assert((zx.cpu.a&31)==28);
 // Flat 48K bus: protect ROM and write the last byte of each RAM bank.
 const uint8_t code[]={0x3e,0x42,0x32,0x00,0x00,0x32,0xff,0x7f,0x32,0xff,0xbf,0x32,0xff,0xff,0x76};
 uint8_t rom0=zx.rom[0][0];memcpy(zx.ram[1],code,sizeof code);
 zx.cpu.iff1=zx.cpu.iff2=false;zx.pins=z80_prefetch(&zx.cpu,0x8000);spectrum_exec(100);
 assert(zx.rom[0][0]==rom0);
 for(int bank=0;bank<3;bank++)assert(zx.ram[bank][16383]==0x42);
 // Snapshot reload releases keys, resumes the saved PC, and renders deterministically.
 spectrum_load_sna(image,sizeof image);assert(!zx.physical_keys[2]&&!zx.physical_keys[3]);
 printf("PASS: SNA validation/registers, 40 keys, keyboard port, deterministic Elite frames, %u audio samples; frame hash %08x; state %zu bytes\n",samples,first,sizeof zx);
 return 0;
}
