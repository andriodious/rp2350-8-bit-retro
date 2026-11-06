"""Exercise the actual C64 packet-only service hook with a deterministic clock."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[2]
s=(root/'src/machines/c64/main.cpp').read_text()
a=s.index('#if C64_FULL_1581\nstatic uint32_t drive_net_calls')
b=s.index('// Scratch 0..3',a)
head=r'''
#include <cstdint>
#include <cassert>
#include <cstdio>
#define C64_FULL_1581 1
#define BOARD_HAS_NET 1
static uint32_t now,packets;
static uint32_t time_us_32(){return now;}
static void net_poll(){packets++;now+=30;}
'''
main=r'''
int main(){
 now=0;retro_drive_network_poll();assert(packets==1 && drive_net_us==30);
 for(now=31;now<5000;now++)retro_drive_network_poll();
 assert(packets==1);
 now=5000;retro_drive_network_poll();assert(packets==2 && drive_net_max_gap==5000);
 now=12000;retro_drive_network_poll();assert(packets==3 && drive_net_max_gap==7000);
 drive_net_calls=0;drive_net_us=0;drive_net_max_gap=0;drive_net_last=0;
 now=0xfffffff0u;retro_drive_network_poll();const auto n=packets;
 now=100;retro_drive_network_poll();assert(packets==n);
 now=4984;retro_drive_network_poll();assert(packets==n+1 && drive_net_max_gap==5000);
 assert(drive_net_us==60);
 puts("Actual C64 poll hook: rate limit, elapsed timing, max gap, reset and timer wrap passed");
}
'''
with tempfile.TemporaryDirectory() as t:
 p=Path(t);(p/'test.cpp').write_text(head+s[a:b]+main)
 subprocess.run(['c++','-std=c++20','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
