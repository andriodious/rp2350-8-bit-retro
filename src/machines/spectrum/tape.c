#include "tape.h"
#include <string.h>
static uint16_t u16(const uint8_t *p){return p[0]|(uint16_t)p[1]<<8;}
static uint32_t u24(const uint8_t *p){return u16(p)|(uint32_t)p[2]<<16;}
static uint32_t u32(const uint8_t *p){return u24(p)|(uint32_t)p[3]<<24;}
// Parse lengths before inspecting payload; never trust a tape's lengths or controls.
static bool block_end(const uint8_t *p,uint32_t n,uint32_t pos,bool tzx,uint32_t *end,const char **err){
 uint32_t h=0,d=0;const uint8_t *q=p+pos;uint32_t r=n-pos;
 if(!tzx){if(r<2)goto truncated;h=2;d=u16(q);if(d<2)goto invalid;}
 else {if(!r)goto truncated;uint8_t id=*q++;r--;pos++;
 #define NEED(x) do{if(r<(x))goto truncated;}while(0)
 switch(id){
 case 0x10:NEED(4);h=4;d=u16(q+2);if(d<2)goto invalid;break;
 case 0x11:NEED(18);h=18;d=u24(q+15);if(!u16(q+6)||!u16(q+8)||q[12]<1||q[12]>8||!d||(u16(q+10)&&!u16(q)))goto invalid;break;
 case 0x12:NEED(4);h=4;if(!u16(q)||!u16(q+2))goto invalid;break;
 case 0x13:NEED(1);h=1;d=2*q[0];break;
 case 0x14:NEED(10);h=10;d=u24(q+7);if(!u16(q)||!u16(q+2)||q[4]<1||q[4]>8||!d)goto invalid;break;
 case 0x15:NEED(8);h=8;d=u24(q+5);if(!u16(q)||q[4]<1||q[4]>8||!d)goto invalid;break;
 case 0x20:case 0x23:h=2;break;
 case 0x24:NEED(2);h=2;if(!u16(q))goto invalid;break;
 case 0x21:case 0x30:NEED(1);h=1;d=q[0];break;
 case 0x22:case 0x25:case 0x27:h=0;break;
 case 0x26:NEED(2);h=2;d=2u*u16(q);if(!d)goto invalid;break;
 case 0x2a:NEED(4);h=4;if(u32(q))goto invalid;break;
 case 0x2b:NEED(5);h=5;if(u32(q)!=1||q[4]>1)goto invalid;break;
 case 0x31:NEED(2);h=2;d=q[1];break;
 case 0x32:NEED(2);h=2;d=u16(q);break;
 case 0x33:NEED(1);h=1;d=3u*q[0];break;
 case 0x35:NEED(20);h=20;d=u32(q+16);break;
 case 0x5a:h=9;break;
 default:*err="Unsupported TZX block (CSW, generalized data and selection menus are not supported)";return false;
 }
 #undef NEED
 }
 if(h>n-pos||d>n-pos-h)goto truncated;
 *end=pos+h+d;
 if(tzx&&p[pos-1]==0x13)for(uint32_t i=0;i<d;i+=2)if(!u16(q+1+i))goto invalid;
 return true;
 truncated:*err="Truncated tape block";return false;
 invalid:*err="Invalid tape pulse, length or control block";return false;
}
bool spectrum_tape_validate(const uint8_t *p,uint32_t n,bool tzx,uint32_t *blocks,const char **err){
 *blocks=0;*err=NULL;if(!p||!n){*err="Empty tape";return false;}
 uint32_t pos=0,depth=0;
 if(tzx){if(n<10||memcmp(p,"ZXTape!\x1a",8)||p[8]!=1){*err="Invalid TZX header/version";return false;}pos=10;}
 while(pos<n){uint32_t end;if(!block_end(p,n,pos,tzx,&end,err))return false;
 if(tzx){if(p[pos]==0x24){if(++depth>8){*err="TZX loops nested too deeply";return false;}}
 else if(p[pos]==0x25){if(!depth){*err="Unmatched TZX loop end";return false;}depth--;}}
 if(++*blocks>8192){*err="Too many tape blocks";return false;}pos=end;
 }
 if(depth||!*blocks){*err="Empty tape or unclosed TZX loop";return false;}
 return true;
}
void spectrum_tape_rewind(spectrum_tape_t *t){
 const uint8_t *p=t->image;uint32_t n=t->size,b=t->blocks;bool tzx=t->tzx;
 memset(t,0,sizeof *t);t->image=p;t->size=n;t->blocks=b;t->tzx=tzx;t->mounted=p!=NULL;t->pos=tzx?10:0;
}
void spectrum_tape_mount(spectrum_tape_t *t,const uint8_t *p,uint32_t n,bool tzx,uint32_t blocks){
 memset(t,0,sizeof *t);t->image=p;t->size=n;t->blocks=blocks;t->tzx=tzx;spectrum_tape_rewind(t);
}
void spectrum_tape_play(spectrum_tape_t *t){if(t->mounted&&!t->error){t->playing=true;if(!t->remaining)spectrum_tape_next(t);}}
static bool seek_block(spectrum_tape_t *t,int64_t dest){
 if(dest<0||dest>=t->blocks){t->error="TZX jump/call outside tape";return false;}
 uint32_t pos=10;
 for(uint32_t b=0;b<(uint32_t)dest;b++)if(!block_end(t->image,t->size,pos,true,&pos,&t->error))return false;
 t->pos=pos;t->block=(uint32_t)dest;return true;
}
// phase: pilot, sync1, sync2, data, pause edge, pause low, next block, pulse list, direct.
void spectrum_tape_next(spectrum_tape_t *t){
 if(!t->playing)return;
 if(t->toggle)t->level=!t->level;
 t->toggle=false;t->remaining=0;
 // Bound zero-time control execution: a malicious jump cycle must not hang the CPU/web.
 for(unsigned budget=0;budget<4096;budget++){
 switch(t->phase){
 case 1:if(t->pilot_count){t->pilot_count--;t->remaining=t->pilot;t->toggle=true;return;}t->phase=2;break;
 case 2:t->phase=3;if(t->sync1){t->remaining=t->sync1;t->toggle=true;return;}break;
 case 3:t->phase=4;if(t->sync2){t->remaining=t->sync2;t->toggle=true;return;}break;
 case 4:
  if(t->bit<t->bits){t->remaining=(t->data[t->bit/8]&(128>>(t->bit%8)))?t->one:t->zero;
   if(++t->half==2){t->half=0;t->bit++;}t->toggle=true;return;}t->phase=5;break;
 case 5:t->phase=6;if(t->pause){t->remaining=3500;return;}break;
 case 6:t->phase=0;if(t->pause){t->level=false;if(t->pause>1){t->remaining=(t->pause-1)*3500;return;}}break;
 case 7:if(t->left){t->remaining=u16(t->pulses);t->pulses+=2;t->left--;t->toggle=true;return;}t->phase=0;break;
 case 8:if(t->bit<t->bits){t->level=(t->data[t->bit/8]&(128>>(t->bit%8)))!=0;t->bit++;t->remaining=t->direct;return;}t->phase=5;break;
 default:{
  if(t->pos==t->size){t->playing=false;return;}
  uint32_t start=t->pos,current=t->block,end;
  if(!block_end(t->image,t->size,start,t->tzx,&end,&t->error))goto fail;
  const uint8_t *q=t->image+start;uint8_t id=t->tzx?*q++:0x10;
  t->pos=end;t->block++;t->bit=t->half=0;t->pause=0;
  switch(id){
  case 0x10:{uint32_t n=u16(q+(t->tzx?2:0));t->data=q+(t->tzx?4:2);t->bits=n*8;
   t->pilot=2168;t->sync1=667;t->sync2=735;t->zero=855;t->one=1710;
   t->pilot_count=t->data[0]<128?8063:3223;t->pause=t->tzx?u16(q):1000;t->phase=1;break;}
  case 0x11:t->pilot=u16(q);t->sync1=u16(q+2);t->sync2=u16(q+4);t->zero=u16(q+6);t->one=u16(q+8);t->pilot_count=u16(q+10);t->pause=u16(q+13);t->bits=(u24(q+15)-1)*8+q[12];t->data=q+18;t->phase=1;break;
  case 0x12:t->pilot=u16(q);t->pilot_count=u16(q+2);t->sync1=t->sync2=0;t->bits=0;t->phase=1;break;
  case 0x13:t->left=q[0];t->pulses=q+1;t->phase=7;break;
  case 0x14:t->zero=u16(q);t->one=u16(q+2);t->bits=(u24(q+7)-1)*8+q[4];t->data=q+10;t->pause=u16(q+5);t->phase=4;break;
  case 0x15:t->direct=u16(q);t->pause=u16(q+2);t->bits=(u24(q+5)-1)*8+q[4];t->data=q+8;t->phase=8;break;
  case 0x20:t->pause=u16(q);if(!t->pause){t->playing=false;return;}t->phase=5;break;
  case 0x23:if(!seek_block(t,(int64_t)current+(int16_t)u16(q)))goto fail;break;
  case 0x24:if(t->depth==8){t->error="TZX loop stack full";goto fail;}t->loop[t->depth].pos=end;t->loop[t->depth].block=t->block;t->loop[t->depth++].left=u16(q);break;
  case 0x25:if(!t->depth){t->error="TZX loop end without start";goto fail;}if(--t->loop[t->depth-1].left){t->pos=t->loop[t->depth-1].pos;t->block=t->loop[t->depth-1].block;}else t->depth--;break;
  case 0x26:{unsigned d=t->call_depth;if(d==8){t->error="TZX call stack full";goto fail;}t->call[d].pos=end;t->call[d].block=t->block;t->call[d].base=current;t->call[d].list=start+3;t->call[d].count=u16(q);t->call[d].index=1;t->call_depth++;if(!seek_block(t,(int64_t)current+(int16_t)u16(q+2)))goto fail;break;}
  case 0x27:{if(!t->call_depth){t->error="TZX return without call";goto fail;}unsigned d=t->call_depth-1;
   if(t->call[d].index<t->call[d].count){uint32_t off=t->call[d].list+2*t->call[d].index++;if(!seek_block(t,(int64_t)t->call[d].base+(int16_t)u16(t->image+off)))goto fail;}
   else{t->pos=t->call[d].pos;t->block=t->call[d].block;t->call_depth--;}break;}
  case 0x2a:t->playing=false;return;
  case 0x2b:t->level=q[4]!=0;break;
  default:break; // Validated metadata only.
  }
  break;
 }
 }
 }
 t->error="TZX control sequence makes no progress";
 fail:t->playing=false;t->remaining=0;
}
