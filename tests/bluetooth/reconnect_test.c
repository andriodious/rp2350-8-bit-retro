#include <assert.h>
#include <stdio.h>
#include "nimbus_reconnect.h"
int main(void){
 uint32_t deadline=nimbus_resume_deadline(false,100,0);
 assert(deadline==3100);
 for(uint32_t now=100;now<3100;now+=10){
   deadline=nimbus_resume_deadline(true,now,deadline);
   assert(deadline==3100);
   assert(!nimbus_retry_due(true,true,false,now,deadline));
 }
 assert(nimbus_retry_due(true,true,false,3100,deadline));
 assert(!nimbus_retry_due(true,true,true,9999,deadline));
 assert(!nimbus_retry_due(true,false,false,9999,deadline));
 assert(!nimbus_retry_due(false,true,false,9999,deadline));
 deadline=nimbus_resume_deadline(false,UINT32_MAX-100,0);
 assert(!nimbus_retry_due(true,true,false,UINT32_MAX-10,deadline));
 assert(nimbus_retry_due(true,true,false,deadline,deadline));
 puts("Reconnect listening window, repeated clicks, busy guard and clock wrap passed");
}
