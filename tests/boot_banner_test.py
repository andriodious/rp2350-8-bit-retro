"""Exercise the real banner renderer and startup lifecycle with simulated hardware.
Optional argument: write a 480x480 PPM preview to that path.
"""
from pathlib import Path
import subprocess, tempfile, sys
root = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as temp:
    t=Path(temp)
    for name, content in {
        'presto_bluetooth.h': 'void presto_bluetooth_boot_poll(void);\nconst char *presto_bluetooth_boot_status(void);\n',
        'board_config.h': '#define BOARD_HAS_NET 1\n#define BOARD_DISPLAY_HEIGHT 480\n#define BOARD_DISPLAY_WIDTH 480\n#define BOARD_DISPLAY_ROW_WORDS 61\n#define BOARD_DISPLAY_ROW_END 8\n',
        'pico/stdlib.h': '#include <stdint.h>\nuint64_t time_us_64(void);\nvoid sleep_ms(unsigned);\n',
        'hardware/watchdog.h': '#include <stdbool.h>\nbool watchdog_caused_reboot(void);\nvoid watchdog_update(void);\n',
    }.items():
        p=t/name;p.parent.mkdir(exist_ok=True,parents=True);p.write_text(content)
    (t/'test.c').write_text(r'''
#include "board.h"
#include "boot_banner.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint32_t fb[2*240*61+1];
static const uint16_t *original[481];
static const uint16_t *const *current=original;
static uint64_t now;
static unsigned switches, polls, feeds;
static uint32_t *visible, *last_visible;
static uint32_t visible_hash;
static uint64_t lit_at, display_duration;
static bool lit;
static bool configured=true;
static uint64_t ip_delay=500000;
static uint32_t checksum(void) {
    uint32_t h=0;
    if(visible) for(int i=0;i<240*61;i++) h=h*31+visible[i];
    return h;
}
static void unchanged(void) { if(visible) assert(checksum()==visible_hash); }
void board_display_backlight(uint8_t b) {
    if(b) { lit=true;lit_at=now; }
    else { if(lit)display_duration=now-lit_at;lit=false; }
}
static bool warm, missing;
uint8_t *drive0_staging(void) { return missing ? NULL : (uint8_t *)fb; }
bool watchdog_caused_reboot(void) { return warm; }
void watchdog_update(void) { feeds++; }
uint64_t time_us_64(void) { return now; }
void sleep_ms(unsigned ms) { now += ms*1000; }
uint32_t board_display_frames(void) { return now/20000; }
const uint16_t *const *board_display_get_rows(void) { return current; }
void board_display_set_rows(const uint16_t *const *rows) {
    unchanged();
    switches++;current=rows;
    if(rows!=original) {
        visible=(uint32_t *)rows[0]; last_visible=visible;
        assert(visible==fb || visible==fb+240*61);
        assert(rows[480]==NULL);
        for(int y=0;y<480;y++) {
            assert(rows[y]==(uint16_t *)(visible+(y/2)*61));
            assert(((uint32_t *)rows[y])[60]==8);
        }
    } else visible=NULL;
    visible_hash=checksum();
}
void net_boot_poll(void) { unchanged();polls++; }
void presto_bluetooth_boot_poll(void) {}
const char *presto_bluetooth_boot_status(void) { return polls>75 ? "CONNECTED" : "SEARCHING"; }
bool net_boot_configured(void) { return configured; }
bool net_boot_has_address(void) { return configured && now-lit_at>=ip_delay; }
void net_boot_status(char *out, size_t n) { snprintf(out,n,"%s",net_boot_has_address()?"192.168.1.42":"getting an address"); }
int main(int argc,char **argv) {
    fb[2*240*61]=0x12345678;
    boot_banner_show();
    assert(display_duration>=3000000 && display_duration<=3060000);
    assert(switches==4 && current==original && polls>=140 && polls<=150 && feeds>=150);
    assert(fb[2*240*61]==0x12345678);
    for(int y=0;y<240;y++) assert(fb[y*61+60]==8);
    if(argc>1) {
        FILE *f=fopen(argv[1],"wb"); assert(f);
        fprintf(f,"P6\n480 480\n255\n");
        for(int y=0;y<480;y++)for(int x=0;x<480;x++) {
            unsigned c=(last_visible[(y/2)*61+x/8]>>((x%8)*4))&7;
            fputc(c&4?255:0,f); fputc(c&2?255:0,f); fputc(c&1?255:0,f);
        }
        fclose(f);
    }
    unsigned before=switches;
    warm=true;
    unsigned old_polls=polls;uint64_t old_now=now;
    boot_banner_show();assert(switches==before && current==original);
    assert(polls==old_polls && now==old_now);
    assert(display_duration>=3000000 && display_duration<=3060000);
    before=switches;
    warm=false;missing=true;boot_banner_show();assert(switches==before);
    missing=false;polls=0;lit=false;now=0;ip_delay=5000000;
    boot_banner_show();
    assert(display_duration>=7000000 && display_duration<=7120000);
    assert(current==original);
    polls=0;lit=false;now=0;ip_delay=UINT64_MAX;
    boot_banner_show();
    assert(display_duration>=30000000 && display_duration<=30060000);
    polls=0;lit=false;now=0;configured=false;
    boot_banner_show();
    assert(display_duration>=3000000 && display_duration<=3060000);
    assert(fb[2*240*61]==0x12345678);
    puts("DHCP delay, two-second address hold, offline timeout, no-credentials timing, banner timing, row format, bounds, restoration, warm-reboot bypass and visible-buffer immutability passed");
}
''')
    subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-DNIMBUS_TEST=1','-DRETRO_BUILD_ID="banner-test"','-I'+str(t),'-I'+str(root/'src/common'),str(root/'src/common/boot_banner.c'),str(t/'test.c'),'-o',str(t/'test')],check=True)
    subprocess.run([str(t/'test'),*sys.argv[1:]],check=True)
