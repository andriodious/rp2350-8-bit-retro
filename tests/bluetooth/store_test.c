#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "mem.h"
#include "controller_store.h"
static unsigned char flash[8192];static int writes,fail;
bool mem_flash_region(int region,uint32_t *off,uint32_t *size){if(region<MEM_FLASH_CONTROLLER_A||region>MEM_FLASH_CONTROLLER_B)return false;*off=(region-MEM_FLASH_CONTROLLER_A)*4096;*size=4096;return true;}
const uint8_t *mem_flash_ptr(uint32_t off){assert(off<8192);return flash+off;}
bool mem_flash_write_sector(uint32_t off,const void *data,uint32_t len){assert(off==0||off==4096);assert(len<4096);writes++;memset(flash+off,255,4096);memcpy(flash+off,data,fail?len/2:len);return !fail;}
int main(void){
 memset(flash,255,sizeof flash);controller_store_init();
 uint8_t a[6]={1,2,3,4,5,6},k[16]={9},outa[6],outk[16],type;
 int32_t cfg[19]={1,2,0,15},out[19];
 assert(!controller_store_bond(outa,outk,&type));assert(!controller_store_get_config(0,out));
 controller_store_set_bond(a,k,4);controller_store_set_config(0,cfg);assert(controller_store_flush());assert(writes==1);
 controller_store_init();assert(controller_store_bond(outa,outk,&type)&&type==4&&!memcmp(k,outk,16));
 assert(controller_store_get_config(0,out)&&!memcmp(out,cfg,sizeof out));
 controller_store_set_bond(a,k,4);controller_store_set_config(0,cfg);assert(controller_store_flush()&&writes==1);
 cfg[0]=2;controller_store_set_config(1,cfg);assert(controller_store_flush());
 controller_store_init();assert(controller_store_get_config(1,out)&&out[0]==2);assert(controller_store_get_config(0,out)&&out[0]==1);
 cfg[0]=0;controller_store_set_config(0,cfg);fail=1;assert(!controller_store_flush());
 controller_store_init();assert(controller_store_get_config(0,out)&&out[0]==1); // previous bank survives interrupted write
 fail=0;controller_store_forget();assert(controller_store_flush());controller_store_init();assert(!controller_store_bond(outa,outk,&type));assert(controller_store_get_config(1,out)&&out[0]==2);
 // Fourth mapping lives in an append-only extension, independent of the legacy three.
 cfg[0]=42;controller_store_set_config(3,cfg);assert(controller_store_flush());controller_store_init();
 assert(controller_store_get_config(3,out)&&out[0]==42);assert(controller_store_get_config(1,out)&&out[0]==2);
 int saved_writes=writes;controller_store_set_config(3,cfg);assert(controller_store_flush()&&writes==saved_writes);
 controller_store_set_config(4,cfg);assert(!controller_store_pending());
 // Old firmware erases the extension when saving its v1 prefix. New firmware
 // must still accept the bond and BBC/C64 settings, without inventing a Spectrum mapping.
 memset(flash+272,255,4096-272);memset(flash+4096+272,255,4096-272);controller_store_init();
 assert(!controller_store_get_config(3,out));assert(controller_store_get_config(1,out)&&out[0]==2);
 // Both copies corrupt => defaults; no unvalidated bond is exposed.
 memset(flash,0,sizeof flash);controller_store_init();assert(!controller_store_bond(outa,outk,&type));assert(!controller_store_get_config(0,out));
 puts("Persistence: reload, per-machine settings, deduplication, interrupted writes, forget and corrupt banks passed");
}
