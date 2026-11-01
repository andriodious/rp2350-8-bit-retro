#include "pixel_convert.h"
#include <cassert>
#include <cstdio>
#include <cstring>
int main(){
 uint16_t palette[16];for(unsigned i=0;i<16;i++)palette[i]=(uint16_t)(i*4093+37);
 alignas(4) uint8_t in[4];uint16_t actual[7],expected[5];
 // Every combination of four palette indices, including ignored high bits.
 for(unsigned word=0;word<65536;word++){
  for(unsigned i=0;i<4;i++)in[i]=((word>>(i*4))&15)|((word>>(i+1))&0xf0);
  c64_convert_four<false>(in,expected,palette);
  for(auto &x:actual)x=0xa55a;
  c64_convert_four<true>(in,actual+1,palette);
  assert(actual[0]==0xa55a && actual[6]==0xa55a);
  assert(!memcmp(expected,actual+1,sizeof expected));
 }
 // Complete 384x272 frame and row padding, using the production row geometry.
 alignas(4) static uint8_t src[384*272];static uint16_t a[272][482],b[272][482];
 uint32_t seed=12345;
 for(auto &x:src){seed=seed*1664525u+1013904223u;x=seed>>24;}
 memset(a,0xa5,sizeof a);memset(b,0xa5,sizeof b);
 for(unsigned y=0;y<272;y++)for(unsigned group=0;group<96;group++){
  c64_convert_four<false>(src+y*384+group*4,a[y]+group*5,palette);
  c64_convert_four<true>(src+y*384+group*4,b[y]+group*5,palette);
 }
 assert(!memcmp(a,b,sizeof a));
 puts("All 65536 palette groups and full-frame pixels/padding match");
}
