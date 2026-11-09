// Host-only C ABI counter/lifecycle test; uses the embedded test ROM object.
#include "drive_bridge.h"
#include <cassert>
#include <cstdlib>
#include <vector>
extern "C" {
void *psram_malloc(size_t n){return malloc(n);}
void psram_free(void *p){free(p);}
void retro_drive_panic(){abort();}
void rust_eh_personality(){abort();}
uint32_t retro_drive_time_us(){static uint32_t t=0xfffffff0u;return t++;}
}
int main(){
 std::vector<uint8_t> disk(819200);
 assert(retro_drive_init(retro_1581_rom,32768,disk.data(),disk.size()));
 assert(retro_drive_instructions()==0);
 for(int i=0;i<10000;i++){retro_drive_add_cycles(1);retro_drive_flush();}
 assert(retro_drive_run_calls()==10000);
 assert(retro_drive_instructions()>0);
 assert(retro_drive_empty_calls()>0 && retro_drive_empty_calls()<10000);
#ifdef DRIVE_PROFILE_TEST
 assert(retro_drive_profile(0)==10000);
 assert(retro_drive_profile(1)>10 && retro_drive_profile(1)<100);
 uint64_t total=0;for(int i=2;i<6;i++)total+=retro_drive_profile(i);
 assert(total>0 && total<retro_drive_run_us());
#else
 assert(retro_drive_run_us()==10000); // Also exercises the 32-bit clock wrap.
 assert(retro_drive_empty_us()==retro_drive_empty_calls());
#endif
 retro_drive_reset();
 for(int i=0;i<6;i++)assert(retro_drive_profile(i)==0);
 assert(retro_drive_idle_loops()==0 && retro_drive_idle_tail_instructions()==0);
 assert(retro_drive_instructions()==0 && retro_drive_run_calls()==0 && retro_drive_empty_calls()==0);
 retro_drive_add_cycles(50);retro_drive_flush();
 assert(retro_drive_instructions()>0);
 retro_drive_close();
 for(int i=0;i<6;i++)assert(retro_drive_profile(i)==0);
 assert(retro_drive_idle_loops()==0 && retro_drive_idle_tail_instructions()==0);
 assert(!retro_drive_active() && retro_drive_instructions()==0 && retro_drive_empty_us()==0);
}
