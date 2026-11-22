"""Exercise the actual firmware retry function with a changing Wi-Fi link."""
from pathlib import Path
import subprocess, tempfile
root=Path(__file__).resolve().parents[2]
s=(root/'src/boards/presto/presto_bluetooth.c').read_text()
a=s.index('static void bluetooth_frame(bool save) {')
b=s.index('\nvoid presto_bluetooth_frame',a)
fn=s[a:b]
net=(root/'src/boards/presto/presto_net.c').read_text()
a=net.index('bool net_bluetooth_ready(void) {')
b=net.index('\nvoid net_boot_status',a)
gate=net[a:b]
source=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "nimbus_reconnect.h"
static bool store_started=true,store_error,ready=true,reconnect=true,scanning,connecting,connected;
static unsigned cid,attempts,error,retry_streak,failures;
static bool preferences_pending,keyboard_busy;
static uint32_t preferences_due;
static void presto_preferences_save(void){}
#define BT_KEYBOARD_DISCOVERY 1
static void keyboard_discovery_frame(void){}
static bool keyboard_discovery_busy(void){return keyboard_busy;}
static uint32_t retry_at=3000,now;
static bool wifi,configured=true,wifi_on=true;
static int link_status;
#define BT_KEYBOARD_CLASSIC 1
static bool keyboard_allowed,host_busy;
static void keyboard_host_frame(bool allowed,bool save){(void)save;keyboard_allowed=allowed;}
static bool keyboard_host_busy(void){return host_busy;}
static bool net_boot_configured(void){return configured;}
static uint32_t time_us_32(void){return now*1000u;}
static unsigned selected;
#define HID_PROTOCOL_MODE_REPORT 1
static bool controller_store_pending(void){return false;}
static bool controller_store_flush(void){return true;}
static uint32_t now_ms(void){return now;}
static bool net_boot_has_address(void){return wifi;}
static bool remembered(unsigned x){(void)x;return true;}
static unsigned hid_host_connect(unsigned a,int b,unsigned *c){(void)a;(void)b;(void)c;return 4;}
static void note_failure(unsigned e,unsigned p){(void)e;(void)p;failures++;}
'''+gate+fn+r'''
int main(void){
 // Both devices wait while Wi-Fi initially associates and requests DHCP.
 for(now=0;now<30000;now+=100){bluetooth_frame(false);assert(!keyboard_allowed);}
 assert(attempts==0 && failures==0 && retry_streak==0 && retry_at==3000);
 bluetooth_frame(false);assert(keyboard_allowed&&attempts==1);
 // Automatic Wi-Fi retries cannot re-lock Bluetooth after the timeout.
 now=31000;bluetooth_frame(false);assert(keyboard_allowed&&attempts==1);
 // Acquiring an address resets the grace period for a later outage.
 wifi=true;bluetooth_frame(false);
 wifi=false;now=40000;bluetooth_frame(false);assert(!keyboard_allowed&&attempts==1);
 now=45000;bluetooth_frame(false);assert(!keyboard_allowed&&attempts==1);
 // An explicit network/authentication failure releases both immediately.
 link_status=-2;bluetooth_frame(false);assert(keyboard_allowed&&attempts==2);
 link_status=1;now=46000;bluetooth_frame(false);assert(keyboard_allowed);
 // Wi-Fi up: preserve listening window and radio-operation arbitration.
 wifi=true;attempts=failures=retry_streak=0;retry_at=50000;now=47000;
 bluetooth_frame(false);assert(attempts==0&&keyboard_allowed);
 now=50000;bluetooth_frame(false);assert(attempts==1);
 scanning=true;now=100000;bluetooth_frame(true);assert(!keyboard_allowed&&attempts==1);
 scanning=false;connecting=true;bluetooth_frame(true);assert(!keyboard_allowed&&attempts==1);
 connecting=false;keyboard_busy=true;bluetooth_frame(true);assert(!keyboard_allowed&&attempts==1);
 keyboard_busy=false;host_busy=true;bluetooth_frame(true);assert(attempts==1);
 host_busy=false;bluetooth_frame(true);assert(attempts==2);
 reconnect=false;now=200000;bluetooth_frame(true);assert(attempts==2);
 // No credentials: allow offline pairing/reconnection without a grace period.
 configured=false;wifi=false;bluetooth_frame(true);assert(keyboard_allowed);
 // Timer wrap must not strand offline devices.
 configured=true;now=4290000;bluetooth_frame(true);assert(!keyboard_allowed);
 now+=30000;bluetooth_frame(true);assert(keyboard_allowed);
 puts("Shared Wi-Fi gate: initial grace, DHCP timeout, failure, offline retries, recovery, no credentials, radio arbitration and timer wrap passed");
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.c').write_text(source)
 subprocess.run(['cc','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(root/'src/boards/presto'),str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
