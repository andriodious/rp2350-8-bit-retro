#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/machines/spectrum/spectrum.c"
static uint8_t extmem[262144],store[1048576],image[200000],packed[200000];
void spectrum_audio(const float *p,int n){(void)p;(void)n;}
static void put16(uint8_t *p,unsigned n){p[0]=n;p[1]=n>>8;}
static uint32_t hash(void){uint32_t h=2166136261;for(unsigned i=0;i<65536;i++)h=(h^address_space[i])*16777619;return h;}
static unsigned rle(uint8_t *out,unsigned size,uint8_t v){unsigned n=0;while(size){unsigned c=size>255?255:size;out[n++]=0xed;out[n++]=0xed;out[n++]=c;out[n++]=v;size-=c;}return n;}
static void snapshots(void){
 memset(image,0,sizeof image);image[0]=0xab;image[1]=0x45;put16(image+6,0x8123);put16(image+8,0xff00);image[12]=3;image[29]=2;image[27]=1;
 for(unsigned i=30;i<49182;i++)image[i]=(uint8_t)i;
 assert(spectrum_load_z80(image,49182));assert(zx.cpu.a==0xab&&zx.cpu.f==0x45&&zx.cpu.im==2&&zx.cpu.r==128&&zx.cpu.iff1&&!zx.cpu.iff2);assert(address_space[16384]==30&&zx.cpu.sp==0xff00);
 uint32_t before=hash();assert(!spectrum_load_z80(image,49181));assert(hash()==before);
 image[12]=32;unsigned n=30+rle(image+30,49152,0xed);memcpy(image+n,"\0\xed\xed\0",4);n+=4;
 assert(spectrum_load_z80(image,n));assert(address_space[65535]==0xed);
 image[32]=0;before=hash();assert(!spectrum_load_z80(image,n));assert(hash()==before);
 for(unsigned ext=23;ext<=55;ext+=(ext==23?31:1)){
  memset(image,0,sizeof image);put16(image+30,ext);put16(image+32,0x9000);image[29]=1;n=32+ext;
  unsigned pages[]={8,4,5};
  for(unsigned b=0;b<3;b++){put16(image+n,65535);image[n+2]=pages[b];memset(image+n+3,0x41+b,16384);n+=16387;}
  assert(spectrum_load_z80(image,n));for(unsigned b=0;b<3;b++)assert(address_space[16384*(b+1)]==0x41+b);
  before=hash();image[34]=4;assert(!spectrum_load_z80(image,n));assert(hash()==before);image[34]=0;
  unsigned k=32+ext;memcpy(packed,image,k);
  for(unsigned b=0;b<3;b++){unsigned len=rle(packed+k+3,16384,0x51+b);put16(packed+k,len);packed[k+2]=pages[b];k+=3+len;}
  assert(spectrum_load_z80(packed,k));assert(address_space[65535]==0x53);
  // Every truncated prefix is rejected without touching the live machine.
  before=hash();for(unsigned j=0;j<k;j++)assert(!spectrum_load_z80(packed,j));assert(hash()==before);
 }
 puts("PASS: Z80 v1/v2/v3 raw/compressed, register and bank restoration, truncated/RLE/128K rejection");
}
static unsigned tap_block(uint8_t *out,const uint8_t *p,unsigned n,uint8_t flag){put16(out,n+2);out[2]=flag;uint8_t sum=flag;for(unsigned i=0;i<n;i++)sum^=(out[i+3]=p[i]);out[n+3]=sum;return n+4;}
static unsigned make_program(uint8_t *out){
 // 10 POKE 32768,42: STOP -- include Spectrum's numeric constants.
 const uint8_t line[]={0,10,27,0,0xf4,'3','2','7','6','8',0x0e,0,0,0,128,0,',','4','2',0x0e,0,0,42,0,0,':',0xe2,13};
 uint8_t program[sizeof line];memcpy(program,line,sizeof line);put16(program+2,sizeof line-4);
 uint8_t header[17]={0};memcpy(header+1,"MEDIA TEST",10);put16(header+11,sizeof program);put16(header+13,10);put16(header+15,sizeof program);
 unsigned n=tap_block(out,header,sizeof header,0);return n+tap_block(out+n,program,sizeof program,255);
}
static void run(unsigned us){while(us){unsigned n=us>1000?1000:us;spectrum_exec(n);us-=n;}}
static void tapes(void){
 unsigned n=make_program(image);uint32_t blocks;const char *err;
 assert(spectrum_tape_validate(image,n,false,&blocks,&err)&&blocks==2);
 spectrum_tape_t t;spectrum_tape_mount(&t,image,n,false,blocks);spectrum_tape_play(&t);
 assert(t.remaining==2168&&!t.level);for(unsigned i=0;i<8063;i++)spectrum_tape_next(&t);assert(t.remaining==667);spectrum_tape_next(&t);assert(t.remaining==735);spectrum_tape_next(&t);assert(t.remaining==855);
 assert(spectrum_load_image("test.TAP",image,n));memset(image,0,n); // retained copy must survive staging overwrite
 run(20000000);printf("TAP integration: PC=%04x POKE=%u state=%s block=%u\n",zx.cpu.pc,address_space[32768],spectrum_tape_state(),tape.block);
 FILE *f=fopen("/tmp/spectrum-tape.ppm","wb");fprintf(f,"P6\n320 256\n255\n");for(unsigned y=0;y<256;y++){uint8_t row[320];spectrum_render_line(y,row);for(unsigned x=0;x<320;x++){unsigned p=row[x],v=(p&8)?255:205;uint8_t rgb[]={p&2?v:0,p&4?v:0,p&1?v:0};fwrite(rgb,1,3,f);}}fclose(f);
 assert(address_space[32768]==42);
 // Same complete load from standard-speed TZX blocks.
 n=make_program(image);memcpy(packed,"ZXTape!\x1a\x01\x14",10);unsigned k=10;
 for(unsigned off=0;off<n;){unsigned size=word(image+off);packed[k++]=0x10;put16(packed+k,1000);k+=2;memcpy(packed+k,image+off,size+2);k+=size+2;off+=size+2;}
 assert(spectrum_load_image("test.tzx",packed,k));address_space[32768]=0;run(20000000);assert(address_space[32768]==42);
 assert(spectrum_tape_command("rewind"));assert(tape.block==0&&!tape.playing);assert(spectrum_tape_command("play"));assert(tape.playing);assert(spectrum_tape_command("stop"));assert(!tape.playing);
 for(unsigned j=0;j<10;j++)assert(!spectrum_tape_validate(packed,j,true,&blocks,&err));
 packed[10]=0x19;assert(!spectrum_load_image("unsupported.tzx",packed,k));assert(tape.mounted);
 puts("PASS: complete TAP and TZX BASIC autoload through ROM EAR input, immutable tape storage, controls");
}
static void controls(void){
 const uint8_t tzx[]={ 'Z','X','T','a','p','e','!',26,1,20,
 0x24,2,0, 0x12,7,0,2,0, 0x25, 0x15,3,0,0,0,3,1,0,0,0xa0, 0x20,0,0};
 uint32_t b;const char *e;assert(spectrum_tape_validate(tzx,sizeof tzx,true,&b,&e));
 spectrum_tape_t t;spectrum_tape_mount(&t,tzx,sizeof tzx,true,b);spectrum_tape_play(&t);
 for(unsigned i=0;i<4;i++){assert(t.remaining==7);spectrum_tape_next(&t);}
 assert(t.remaining==3&&t.level);spectrum_tape_next(&t);assert(t.remaining==3&&!t.level);spectrum_tape_next(&t);assert(t.remaining==3&&t.level);spectrum_tape_next(&t);assert(!t.playing);
 const uint8_t cycle[]={'Z','X','T','a','p','e','!',26,1,20,0x23,0,0};
 assert(spectrum_tape_validate(cycle,sizeof cycle,true,&b,&e));spectrum_tape_mount(&t,cycle,sizeof cycle,true,b);spectrum_tape_play(&t);assert(t.error&&!t.playing);
 puts("PASS: TZX loops, direct sample levels, stop blocks and bounded jump cycles");
}


static void elite_snapshot(void){
 extern const uint8_t spectrum_elite[];extern const unsigned spectrum_elite_size;
 const uint8_t *p=spectrum_elite;unsigned sp=word(p+23);
 memset(image,0,30);image[0]=p[22];image[1]=p[21];memcpy(image+2,p+13,2);memcpy(image+4,p+9,2);
 put16(image+6,word(p+27+sp-0x4000));put16(image+8,sp+2);image[10]=p[0];image[11]=p[20]&127;image[12]=(p[20]>>7)|(p[26]<<1);
 memcpy(image+13,p+11,2);memcpy(image+15,p+5,2);memcpy(image+17,p+3,2);memcpy(image+19,p+1,2);image[21]=p[8];image[22]=p[7];
 memcpy(image+23,p+15,4);image[27]=image[28]=(p[19]&4)!=0;image[29]=p[25];memcpy(image+30,p+27,49152);
 assert(spectrum_load_sna(p,spectrum_elite_size));run(5000000);uint32_t expected=hash();uint16_t pc=zx.cpu.pc;
 assert(spectrum_load_z80(image,49182));run(5000000);assert(hash()==expected&&zx.cpu.pc==pc);
 puts("PASS: Elite Z80 snapshot produces identical RAM and CPU position to built-in SNA after 5 seconds");
}

static void more_tzx(void){
 uint32_t b;const char *e;spectrum_tape_t t;
 // Turbo: 2 pilot, two sync, one data bit (two equal pulses), no pause.
 const uint8_t turbo[]={'Z','X','T','a','p','e','!',26,1,20,0x11,10,0,11,0,12,0,13,0,14,0,2,0,1,0,0,1,0,0,0x80};
 assert(spectrum_tape_validate(turbo,sizeof turbo,true,&b,&e));spectrum_tape_mount(&t,turbo,sizeof turbo,true,b);spectrum_tape_play(&t);
 unsigned expected[]={10,10,11,12,14,14};for(unsigned i=0;i<6;i++){assert(t.remaining==expected[i]);spectrum_tape_next(&t);}assert(!t.playing);
 // Pure data, pulse sequence, signal set, pause, 48K stop; all are bounded.
 const uint8_t pure[]={'Z','X','T','a','p','e','!',26,1,20,0x14,5,0,9,0,2,0,0,1,0,0,0x40,0x13,2,6,0,7,0,0x2b,1,0,0,0,1,0x20,2,0,0x2a,0,0,0,0};
 assert(spectrum_tape_validate(pure,sizeof pure,true,&b,&e));spectrum_tape_mount(&t,pure,sizeof pure,true,b);spectrum_tape_play(&t);
 unsigned exp2[]={5,5,9,9,6,7,3500,3500};for(unsigned i=0;i<8;i++){assert(t.remaining==exp2[i]);spectrum_tape_next(&t);}assert(!t.playing&&!t.level);
 // Call block 2, return to jump block 1, jump to stop block 4.
 const uint8_t calls[]={'Z','X','T','a','p','e','!',26,1,20,0x26,1,0,2,0,0x23,3,0,0x12,15,0,1,0,0x27,0x2a,0,0,0,0};
 assert(spectrum_tape_validate(calls,sizeof calls,true,&b,&e));spectrum_tape_mount(&t,calls,sizeof calls,true,b);spectrum_tape_play(&t);assert(t.remaining==15);spectrum_tape_next(&t);assert(!t.playing&&!t.error&&t.block==5);
 // Reproducible malformed-input corpus, including valid headers with random blocks.
 uint32_t seed=12345;
 for(unsigned i=0;i<20000;i++){
  seed=seed*1664525+1013904223;unsigned n=seed%1024;
  for(unsigned j=0;j<n;j++){seed=seed*1664525+1013904223;packed[j]=seed>>24;}
  if(n>=10&&i%2)memcpy(packed,"ZXTape!\x1a\x01\x14",10);
  if(spectrum_tape_validate(packed,n,i%2,&b,&e)){spectrum_tape_mount(&t,packed,n,i%2,b);spectrum_tape_play(&t);for(unsigned j=0;j<100&&t.playing;j++)spectrum_tape_next(&t);}
  spectrum_load_z80(packed,n);
 }
 puts("PASS: turbo/pure/pulse TZX timings, call/return/jump, 48K stop, 20,000 malformed inputs");
}

static void web_joystick_test(void){
 const unsigned masks[]={8,4,2,1,16};
 for(unsigned i=0;i<5;i++){spectrum_key(129+i,true);assert(zx.joy_joymask==masks[i]);spectrum_key(129+i,false);assert(zx.joy_joymask==0);}
 spectrum_key(129,true);spectrum_key(132,true);spectrum_key(133,true);assert(zx.joy_joymask==25);
 spectrum_joystick(4,false);assert(zx.joy_joymask==27);
 spectrum_joystick(0,false);assert(zx.joy_joymask==25);
 spectrum_key(129,false);spectrum_key(132,false);spectrum_key(133,false);assert(zx.joy_joymask==0);
 spectrum_joystick(1,true);spectrum_key(129,true);spectrum_key(129,false);assert(zx.joy_joymask==24);
 spectrum_joystick(0,false);assert(zx.joy_joymask==0);
 puts("PASS: Kempston web directions/fire, simultaneous inputs and independent Nimbus releases");
}
static void reset_to_basic_test(void){
 spectrum_init(extmem);run(3000000);uint32_t expected=hash();uint16_t pc=spectrum_pc();
 extern const uint8_t spectrum_elite[];extern const unsigned spectrum_elite_size;
 assert(spectrum_load_sna(spectrum_elite,spectrum_elite_size));run(100000);
 spectrum_key(129,true);spectrum_key(0,true);spectrum_joystick(8,true);
 spectrum_reset();assert(zx.cpu.pc==0);assert(!zx.joy_joymask);
 for(unsigned i=0;i<8;i++)assert(!zx.physical_keys[i]);
 run(3000000);assert(hash()==expected);assert(spectrum_pc()==pc);
 puts("PASS: Reset after Elite returns to identical fresh ROM/BASIC state and releases controls");
}
int main(void){spectrum_init(extmem);spectrum_media_storage(store,sizeof store);web_joystick_test();snapshots();controls();more_tzx();tapes();elite_snapshot();reset_to_basic_test();return 0;}
