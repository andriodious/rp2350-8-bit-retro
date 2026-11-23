"""Exercise actual radio startup/poll: Bluetooth cannot bypass the DHCP gate."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[2]
s=(root/'src/boards/presto/presto_net.c').read_text()
def extract(a,b):return s[s.index(a):s.index(b,s.index(a))]
bt=(root/'src/boards/presto/presto_bluetooth.c').read_text()
status=bt[bt.index('const char *presto_bluetooth_boot_status(void) {'):bt.index('\nvoid presto_bluetooth_prepare_restart')]
source=r'''
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
typedef unsigned bd_addr_t;
static bool connected,connecting,descriptor_ready,ready,reconnect=true,store_started=true;
static bool remembered(bd_addr_t a){(void)a;return true;}
#define NIMBUS_TEST 1
#define CYW43_COUNTRY_UK 0
#define CYW43_NONE_PM 0
static int cyw43_state;
static bool server_pending;
static void start_server(void){}
static void startup_diag_mark(int n){(void)n;}
static struct {uint32_t polls,poll_at,poll_gap,poll_cost;} nd;
typedef struct {char *ssid,*pass;} cred_t;
static cred_t credentials={"test","test"};
static bool configured=true,radio_on,wifi_on,address,init_failed;
static uint32_t now,last_poll_us,last_link_check_us;
static unsigned bt_starts,joins,polls,initializations;
static char ssid_ram[64],pass_ram[64];
static const cred_t *saved_cred(void){return configured?&credentials:NULL;}
static void net_start_step(int n){(void)n;}
static int cyw43_arch_init_with_country(int n){(void)n;initializations++;return init_failed;}
static void cyw43_arch_enable_sta_mode(void){}
static void cyw43_wifi_pm(int *p,int n){(void)p;(void)n;}
static void join(void){joins++;}
static uint32_t time_us_32(void){return now;}
static void cyw43_arch_poll(void){polls++;}
static void check_link(void){} // failed or pending association never opens the gate
static bool net_boot_has_address(void){return address;}
static bool net_boot_configured(void){return configured;}
static void presto_bluetooth_start(void){bt_starts++;}
'''+extract('bool net_bluetooth_ready(void) {','\nvoid net_boot_status')+extract('static void net_start(bool boot_banner) {','\nstatic void check_link')+extract('void net_poll(void) {','\nstatic const char *link_name')+status+r'''
int main(int argc,char **argv){
 if(argc>1&&argv[1][0]=='o')configured=false;
 if(argc>1&&argv[1][0]=='f')init_failed=true;
 net_start(true);assert(initializations==1&&bt_starts==0);
 if(configured)assert(!strcmp(presto_bluetooth_boot_status(),"WAITING FOR WIFI"));
 if(init_failed){now=40000000;net_poll();assert(!radio_on&&!bt_starts);return 0;}
 assert(radio_on&&joins==(configured?1u:0u));
 now=1000;net_poll();assert(bt_starts==(configured?0u:1u));
 if(configured){
  now=29999999;net_poll();assert(!bt_starts);
  if(argc>1&&argv[1][0]=='d')address=true;
  else {now=29999999;net_poll();assert(!bt_starts);now=30001000;}
  net_poll(); // DHCP path needs a new polling interval
  now+=1000;net_poll();assert(bt_starts==1);
 }
 now+=40000000;net_poll();assert(bt_starts==1&&polls>=2);
}
'''
with tempfile.TemporaryDirectory() as directory:
 p=Path(directory);(p/'test.c').write_text(source)
 subprocess.run(['cc','-std=c11','-fsanitize=address,undefined',str(p/'test.c'),'-o',str(p/'test')],check=True)
 for case in ['timeout','dhcp','offline','failed-init']:
  subprocess.run([str(p/'test'),case],check=True)
print('Radio startup: DHCP, full timeout, no credentials, failed init, exactly-once Bluetooth start: PASS')
