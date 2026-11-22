// A bounded power-on banner. The upload staging area is idle until the HTTP
// listener is enabled after this function returns. No permanent framebuffer RAM.
#include "boot_banner.h"
#include "c64_runtime_diag.h"
#include "board.h"
#include "drive.h"
#include "machine_if.h"
#include "net_if.h"
#include "pico/stdlib.h"
#include "hardware/watchdog.h"
#include "hardware/sync.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#if NIMBUS_TEST
#include "presto_bluetooth.h"
#endif
#include "boot_logo.h"
#if WITH_SOUND
#include "sound.h"
#endif
#ifndef RETRO_BUILD_ID
#define RETRO_BUILD_ID "development"
#endif

// Original compact 5x7 uppercase bitmap alphabet, one row per byte.
static const char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 .:-/!\"#$%&'()<>?+*=;,@[]^_{}~|";
static const uint8_t glyphs[][7] = {
 {14,17,17,31,17,17,17},{30,17,17,30,17,17,30},{14,17,16,16,16,17,14},
 {30,17,17,17,17,17,30},{31,16,16,30,16,16,31},{31,16,16,30,16,16,16},
 {14,17,16,23,17,17,15},{17,17,17,31,17,17,17},{14,4,4,4,4,4,14},
 {7,2,2,2,18,18,12},{17,18,20,24,20,18,17},{16,16,16,16,16,16,31},
 {17,27,21,21,17,17,17},{17,25,21,19,17,17,17},{14,17,17,17,17,17,14},
 {30,17,17,30,16,16,16},{14,17,17,17,21,18,13},{30,17,17,30,20,18,17},
 {15,16,16,14,1,1,30},{31,4,4,4,4,4,4},{17,17,17,17,17,17,14},
 {17,17,17,17,17,10,4},{17,17,17,21,21,21,10},{17,17,10,4,10,17,17},
 {17,17,10,4,4,4,4},{31,1,2,4,8,16,31},
 {14,17,19,21,25,17,14},{4,12,4,4,4,4,14},{14,17,1,2,4,8,31},
 {30,1,1,14,1,1,30},{2,6,10,18,31,2,2},{31,16,16,30,1,1,30},
 {14,16,16,30,17,17,14},{31,1,2,4,8,8,8},{14,17,17,14,17,17,14},
 {14,17,17,15,1,1,14},{0,0,0,0,0,0,0},{0,0,0,0,0,12,12},
 {0,12,12,0,12,12,0},{0,0,0,31,0,0,0},{1,2,2,4,8,8,16},
 {4,4,4,4,4,0,4},
 {10,10,10,0,0,0,0},
 {10,31,10,10,31,10,0},
 {4,15,20,14,5,30,4},
 {25,26,2,4,8,11,19},
 {12,18,20,8,21,18,13},
 {4,4,8,0,0,0,0},
 {2,4,8,8,8,4,2},
 {8,4,2,2,2,4,8},
 {2,4,8,16,8,4,2},
 {8,4,2,1,2,4,8},
 {14,17,1,2,4,0,4},
 {0,4,4,31,4,4,0},
 {0,21,14,31,14,21,0},
 {0,0,31,0,31,0,0},
 {0,12,12,0,12,4,8},
 {0,0,0,0,12,4,8},
 {14,17,23,21,23,16,14},
 {14,8,8,8,8,8,14},
 {14,2,2,2,2,2,14},
 {4,10,17,0,0,0,0},
 {0,0,0,0,0,0,31},
 {2,4,4,8,4,4,2},
 {8,4,4,2,4,4,8},
 {0,0,9,22,0,0,0},
 {4,4,4,4,4,4,4}
};
#define ROW_BYTES (BOARD_DISPLAY_ROW_WORDS * 4)
#define BANNER_ROWS 240
#define BORROW_STATUS_FB (MACHINE_C64 || MACHINE_SPECTRUM || PRESTO_BEEBJIT_WIRE)
_Static_assert(2 * BANNER_ROWS * ROW_BYTES <= DRIVE_IMAGE_MAX, "banner exceeds upload staging");

static void pixel(uint32_t *fb, int x, int y, unsigned colour) {
    if (x < 0 || x >= 480 || y < 0 || y >= BANNER_ROWS) return;
#if MACHINE_C64 || MACHINE_SPECTRUM
    uint32_t rgb = ((colour & 4) ? 0xf800 : 0) | ((colour & 2) ? 0x7e0 : 0) | ((colour & 1) ? 31 : 0);
    uint32_t pins;
    __asm("rbit %0, %1" : "=r"(pins) : "r"(rgb << 16));
    ((uint16_t *)(fb + y * BOARD_DISPLAY_ROW_WORDS))[x] = pins;
#else
    uint32_t *p = fb + y * BOARD_DISPLAY_ROW_WORDS + x / 8;
    unsigned shift = (x % 8) * 4;
    *p = (*p & ~(15u << shift)) | ((colour & 7u) << shift);
#endif
}
static unsigned text_ink=7;
static void text(uint32_t *fb, int x, int y, const char *s) {
    for (; *s && x + 10 <= 464; s++, x += 12) {
        const char *g = strchr(alphabet, toupper((unsigned char)*s));
        if (!g) continue;
        for (int yy = 0; yy < 7; yy++) for (int xx = 0; xx < 5; xx++)
            if (glyphs[g-alphabet][yy] & (16 >> xx)) {
                pixel(fb, x+xx*2, y+yy, text_ink); pixel(fb, x+xx*2+1, y+yy, text_ink);
            }
    }
}
static void read_status(char *value, size_t size, char *line, size_t line_size) {
#if BOARD_HAS_NET
    net_boot_status(value, size);
#else
    snprintf(value, size, "UNAVAILABLE");
#endif
#if NIMBUS_TEST
    #if BT_KEYBOARD_CLASSIC
    snprintf(line, line_size, "JOYSTICK: %s\nKEYBOARD: %s", presto_bluetooth_boot_status(), presto_keyboard_boot_status());
#else
    snprintf(line, line_size, "JOYSTICK: %s", presto_bluetooth_boot_status());
#endif
#else
    snprintf(line, line_size, "JOYSTICK: DISABLED IN BUILD");
#endif
}
static void status(uint32_t *fb, const char *value, const char *line) {
    // Only rewrite status rows; the logo and build text stay stable.
    for (int y = 110; y < 180; y++) memset(fb + y * BOARD_DISPLAY_ROW_WORDS, 0, ROW_BYTES - 4);
    text(fb, 20, 114, "DHCP IP ADDRESS");
    text(fb, 20, 127, value);
    const char *second = strchr(line, '\n');
    char first[64];
    snprintf(first, sizeof first, "%.*s", second ? (int)(second-line) : (int)strlen(line), line);
    text(fb, 20, 155, first);
    if(second)text(fb, 20, 168, second+1);
}
static void retire_display_table(void) {
    uint32_t frame = board_display_frames();
    while ((uint32_t)(board_display_frames() - frame) < 2) {
        watchdog_update();
        sleep_ms(1);
    }
}
// Publish only immutable SRAM pixels: PSRAM scanout can starve the LCD FIFO.
#define BOOT_BANK_ROWS 96
static bool boot_rows_pack(const uint32_t *src,uint32_t *bank,const uint16_t **rows) {
    unsigned used=0;
    for(unsigned y=0;y<BANNER_ROWS;y++) {
        unsigned match=0;
        for(;match<used;match++)if(!memcmp(bank+match*BOARD_DISPLAY_ROW_WORDS,src+y*BOARD_DISPLAY_ROW_WORDS,ROW_BYTES))break;
        if(match==used) {
            if(used==BOOT_BANK_ROWS)return false;
            memcpy(bank+used++*BOARD_DISPLAY_ROW_WORDS,src+y*BOARD_DISPLAY_ROW_WORDS,ROW_BYTES);
        }
        rows[y*2]=rows[y*2+1]=(const uint16_t *)(bank+match*BOARD_DISPLAY_ROW_WORDS);
    }
    rows[BOARD_DISPLAY_HEIGHT]=NULL;
    return true;
}
void boot_banner_show(void) {
    // Web machine switches use a watchdog restart: leave their network and
    // controller startup path untouched, and skip recovery restarts too.
    if (watchdog_caused_reboot()) return;
    // Recovery paths without an initialized display/staging area safely skip it.
    uint32_t *fb = (uint32_t *)drive0_staging();
    const uint16_t *const *previous = board_display_get_rows();
    if (!fb || !previous) return;
    const uint16_t **rows = malloc(2 * (BOARD_DISPLAY_HEIGHT + 1) * sizeof *rows);
    if (!rows) return; // Failure must never prevent the emulator from booting.
    const size_t bank_bytes=2*BOOT_BANK_ROWS*ROW_BYTES;
#if BOOT_STATUS_TRIAL && BORROW_STATUS_FB
    // Rendering has not started; borrow and preserve the existing SRAM framebuffer.
    uint32_t *banks=machine_status_framebuffer(true);
    void *saved=board_psram()+board_psram_size()-BOARD_STATUS_BYTES;
    memcpy(saved,banks,bank_bytes);
#else
    uint32_t *banks=malloc(bank_bytes);
    if(!banks){free(rows);return;}
#endif
    const unsigned words = BANNER_ROWS * BOARD_DISPLAY_ROW_WORDS;
    uint32_t *buffers[2] = {fb, fb + words};
    const uint16_t **tables[2] = {rows, rows + BOARD_DISPLAY_HEIGHT + 1};
    memset(fb, 0, 2 * BANNER_ROWS * ROW_BYTES);
    for (int y = 0; y < BANNER_ROWS; y++) {
#if !MACHINE_C64 && !MACHINE_SPECTRUM
        fb[(y + 1) * BOARD_DISPLAY_ROW_WORDS - 1] = BOARD_DISPLAY_ROW_END;
#endif
        rows[y*2] = rows[y*2+1] = (const uint16_t *)(fb + y * BOARD_DISPLAY_ROW_WORDS);
    }
    rows[480] = NULL;
    for (int y = 0; y < 480; y++) tables[1][y] = (const uint16_t *)(buffers[1] + (y/2) * BOARD_DISPLAY_ROW_WORDS);
    tables[1][480] = NULL;
    for (int y=0; y<48; y++) for (int x=0; x<48; x++) {
        pixel(fb, 16+x*2, 8+y, boot_logo[y*48+x]);
        pixel(fb, 17+x*2, 8+y, boot_logo[y*48+x]);
    }
    text(fb, 132, 20, "RP2350");
    text(fb, 132, 34, "8-BIT RETRO");
    text(fb, 20, 72, "BUILD " RETRO_BUILD_ID);
    text(fb, 20, 86, __DATE__ " " __TIME__);
    text(fb, 20, 196, "WEB: 8BITRETRO.LOCAL");
    text(fb, 20, 216, "STARTING EMULATOR...");
    char value[48], line[64], last_value[48], last_line[64];
    read_status(value, sizeof value, line, sizeof line);
    status(fb, value, line);
    strcpy(last_value, value); strcpy(last_line, line);
    // Never expose an old emulator scanline/cursor while changing row tables.
    board_display_backlight(0);
    if(!boot_rows_pack(fb,banks,rows))goto restore_banner;
    board_display_set_rows(rows);
    retire_display_table();
    board_display_backlight(255);
    unsigned active = 0;
    const uint64_t started = time_us_64();
    const uint64_t minimum_until = started + 3000000;
    const uint64_t network_timeout = started + 30000000;
    uint64_t address_until = 0;
    bool wait_for_address = false;
#if BOARD_HAS_NET
    wait_for_address = net_boot_configured();
#endif
    do {
        watchdog_update();
#if BOARD_HAS_NET
        net_boot_poll();
#endif
#if NIMBUS_TEST
        presto_bluetooth_boot_poll();
#endif
        read_status(value, sizeof value, line, sizeof line);
        if (strcmp(value, last_value) || strcmp(line, last_line)) {
            unsigned next = active ^ 1u;
            memcpy(buffers[next], buffers[active], BANNER_ROWS * ROW_BYTES);
            status(buffers[next], value, line);
            if(!boot_rows_pack(buffers[next],banks+next*BOOT_BANK_ROWS*BOARD_DISPLAY_ROW_WORDS,tables[next]))break;
            board_display_set_rows(tables[next]);
            // The previous buffer must leave DMA before it can be drawn into.
            retire_display_table();
            active = next;
            strcpy(last_value, value); strcpy(last_line, line);
        }
#if BOARD_HAS_NET
        // Start the reading interval only after the IP text has reached scanout.
        if (wait_for_address && !address_until && net_boot_has_address())
            address_until = time_us_64() + 2000000;
#endif
        uint64_t now = time_us_64();
        bool network_done = !wait_for_address ||
            (address_until ? now >= address_until : now >= network_timeout);
        if (now >= minimum_until && network_done) break;
        sleep_ms(20);
    } while (true);
restore_banner:
    board_display_backlight(0);
#if BOOT_STATUS_TRIAL && BORROW_STATUS_FB
    // Stop scanning borrowed pixels before restoring them.
    board_display_set_rows(previous);
    retire_display_table();
    memcpy(banks,saved,bank_bytes);
    machine_status_framebuffer(false);
#endif
    board_display_set_rows(previous);
    retire_display_table();
    board_display_backlight(255);
#if !(BOOT_STATUS_TRIAL && BORROW_STATUS_FB)
    free(banks);
#endif
    free(rows);
}

#if BOOT_STATUS_TRIAL
#include "boot_button.h"
#include "system.h"
#include "api/api.h"
#include "firmware_identity.h"
#if NIMBUS_TEST
#include "controller.h"
#endif
// Runtime buffers must not borrow the upload staging area. Every machine reserves
// the top 512 KiB of PSRAM in this trial, including Beeb's disc and big allocator.
_Static_assert(2 * BANNER_ROWS * ROW_BYTES <= BOARD_STATUS_BYTES, "status reservation too small");
static const uint16_t **runtime_rows;
static const uint16_t *const *emulator_rows;
static bool status_visible;
static uint32_t *status_sram;
#define STATUS_BANK_ROWS 96
static unsigned status_active;
static uint32_t status_frame, status_updated_ms;

// Root-directory titles use the existing machine-specific SD extension filter.
// The last 60 KiB of the reserved area is separate from both status buffers.
#define TITLE_LIMIT 240
#define TITLE_NAME_BYTES 256
_Static_assert(2 * BANNER_ROWS * ROW_BYTES + TITLE_LIMIT * TITLE_NAME_BYTES <= BOARD_STATUS_BYTES,
               "title list overlaps status buffers");
static unsigned title_count, title_selected;
static bool chooser, chooser_dirty, title_limit, fire_armed;
static bool machine_chooser;
static bool touch_select;
static unsigned machine_selected;
static const char *const machine_ids[]={"master","beeb","c64","spectrum"};
static int held_direction;
static uint32_t repeat_at;
static void *title_iterator;
static const char *load_error;
static char (*titles(void))[TITLE_NAME_BYTES] {
    return (char (*)[TITLE_NAME_BYTES])(board_psram()+board_psram_size()-TITLE_LIMIT*TITLE_NAME_BYTES);
}
static void titles_end(void) {
    if(title_iterator)board_sd_discs_end(title_iterator);
    title_iterator=NULL;
}
static bool joystick_state(unsigned *directions,bool *fire) {
#if NIMBUS_TEST
    bool joystick=controller_menu_state(directions,fire);
    unsigned kd;bool kf;controller_keyboard_menu_state(&kd,&kf);
    *directions|=kd;*fire|=kf;
    if((*directions & 3u)==3u)*directions &= ~3u;
#if BT_KEYBOARD_CLASSIC
    return joystick || !strcmp(presto_keyboard_boot_status(),"CONNECTED");
#else
    return joystick;
#endif
#else
    *directions=0;*fire=false;return false;
#endif
}
static void machines_begin(void) {
    titles_end();chooser=false;machine_chooser=system_launched();
    machine_selected=0;
    for(unsigned i=0;i<4;i++)if(!strcmp(machine_name(),machine_ids[i]))machine_selected=i;
    held_direction=0;fire_armed=false;chooser_dirty=true;
}
static int title_compare(const void *a,const void *b){return strcasecmp(a,b);}
static void titles_begin(void) {
    titles_end();machine_chooser=false;title_count=title_selected=0;title_limit=false;load_error=NULL;
    held_direction=0;fire_armed=false;
    chooser=true;
    title_iterator=board_sd_discs_begin();
}
static void titles_poll(void) {
    if(!title_iterator)return;
    for(unsigned i=0;i<8;i++) {
        char name[TITLE_NAME_BYTES];uint32_t size;
        if(!board_sd_discs_next(title_iterator,name,sizeof name,&size)){
            titles_end();break;
        }
        if(!size || size>DRIVE_IMAGE_MAX)continue;
        if(title_count==TITLE_LIMIT){title_limit=true;titles_end();break;}
        snprintf(titles()[title_count++],TITLE_NAME_BYTES,"%s",name);
    }
    if(!title_iterator){
        qsort(titles(),title_count,TITLE_NAME_BYTES,title_compare);

        chooser_dirty=true;
    }
}

static bool status_allocate(void) {
    if (runtime_rows) return true;
    if (!board_display_get_rows() || !board_psram() || board_psram_size() < BOARD_STATUS_BYTES) return false;
    runtime_rows = malloc(2 * (BOARD_DISPLAY_HEIGHT + 1) * sizeof *runtime_rows);
    if (!runtime_rows) return false;
#if !BORROW_STATUS_FB
    // Packed-4 status uses 46 KiB for two compact banks; emulator drawing
    // continues in its own buffer. Never scan status pixels from PSRAM.
    status_sram=malloc(2*STATUS_BANK_ROWS*ROW_BYTES);
    if(!status_sram){free(runtime_rows);runtime_rows=NULL;return false;}
#endif
    return true;
}

static void box(uint32_t *fb,int x,int y,int w,int h,unsigned fill,unsigned edge) {
    for(int yy=0;yy<h;yy++)for(int xx=0;xx<w;xx++)
        pixel(fb,x+xx,y+yy,(xx==0||xx==w-1||yy==0||yy==h-1)?edge:fill);
}
static void ui_button(uint32_t *fb,int x,int y,int w,int h,const char *label,unsigned fill) {
    box(fb,x,y,w,h,fill,7);
    text_ink=fill==3?0:7;
    text(fb,x+(w-(int)strlen(label)*12)/2,y+(h-7)/2,label);
    text_ink=7;
}
static void centred_text(uint32_t *fb,int y,const char *label,bool bold) {
    int width=(int)strlen(label)*12-2+(bold?1:0);
    int x=(480-width)/2;
    text(fb,x,y,label);
    if(bold)text(fb,x+1,y,label);
}
static void slider(uint32_t *fb,unsigned y,unsigned value) {
    box(fb,20,y,440,6,0,7);
    for(unsigned yy=y+1;yy<y+5;yy++)for(unsigned x=21;x<21+value*438/100;x++)pixel(fb,x,yy,3);
    box(fb,20+value*420/100,y-3,20,12,7,3);
}
#include "boot_setup.inc"
#include "boot_touch_keyboard.inc"

static void status_draw(unsigned buffer) {
    uint32_t *fb=(uint32_t *)(board_psram()+board_psram_size()-BOARD_STATUS_BYTES);
    (void)buffer;fb+=BANNER_ROWS*BOARD_DISPLAY_ROW_WORDS;
    memset(fb,0,BANNER_ROWS*ROW_BYTES);
#if !MACHINE_C64 && !MACHINE_SPECTRUM
    for(unsigned y=0;y<BANNER_ROWS;y++)fb[(y+1)*BOARD_DISPLAY_ROW_WORDS-1]=BOARD_DISPLAY_ROW_END;
#endif
#if NIMBUS_TEST && BT_KEYBOARD_CLASSIC
    if(touch_keyboard){touch_key_draw(fb);return;}
    if(setup_page){setup_draw(fb);return;}
#endif
    ui_button(fb,8,214,148,24,chooser?"KEYBOARD":"PREVIOUS",0);
    ui_button(fb,166,214,148,24,"NEXT",0);
    ui_button(fb,324,214,148,24,"EXIT",4);
    char line[64];
    if(machine_chooser) {
        centred_text(fb,8,"CONFIGURATION",true);
        const char *labels[]={"BBC MASTER","BBC MICRO","COMMODORE 64","ZX SPECTRUM"};
        for(unsigned i=0;i<4;i++)ui_button(fb,12+(i%2)*234,28+(i/2)*34,222,30,labels[i],i==machine_selected?3:0);
        bool on,led,locks;unsigned vol,bright;
        audio_get(&on,&vol);board_leds_get(&led,&bright,&locks);
        ui_button(fb,12,104,170,24,on?"SOUND ON":"SOUND OFF",0);
        snprintf(line,sizeof line,"VOLUME %u",vol);text(fb,200,113,line);slider(fb,139,vol);
        ui_button(fb,12,158,170,24,led?"LEDS ON":"LEDS OFF",0);
        snprintf(line,sizeof line,"BRIGHTNESS %u",bright);text(fb,200,167,line);slider(fb,193,bright);
        return;
    }
    if(chooser) {
        centred_text(fb,6,"SD TITLES",true);text(fb,20,19,machine_name());
        if(title_iterator)text(fb,20,70,"READING SD CARD...");
        else if(!title_count)text(fb,20,70,"NO COMPATIBLE SD TITLES");
        else {
            unsigned start=(title_selected/5)*5;
            for(unsigned i=start;i<title_count&&i<start+5;i++) {
                snprintf(line,sizeof line,"%.35s",titles()[i]);
                ui_button(fb,12,35+(i-start)*25,456,22,line,i==title_selected?3:0);
            }
            snprintf(line,sizeof line,"%u OF %u",title_selected+1,title_count);text(fb,170,166,line);
        }
        ui_button(fb,8,183,148,24,"UP",0);ui_button(fb,166,183,148,24,"DOWN",0);ui_button(fb,324,183,148,24,"BOOT",3);
        return;
    }
    centred_text(fb,8,"SYSTEM STATUS",true);
    snprintf(line,sizeof line,"MACHINE: %s",machine_name());text(fb,20,33,line);
    text(fb,20,52,"IMAGE " RETRO_IMAGE_ID);
    // The paused status page has its own compact layout; startup reserves space for the logo.
    char value[48],joystick[64];read_status(value,sizeof value,joystick,sizeof joystick);
    text(fb,20,76,"DHCP IP ADDRESS");text(fb,20,89,value);
    const char *keyboard=strchr(joystick,'\n');
    snprintf(line,sizeof line,"%.*s",keyboard?(int)(keyboard-joystick):(int)strlen(joystick),joystick);
    text(fb,20,115,line);
    if(keyboard)text(fb,20,134,keyboard+1);
#if NIMBUS_TEST && BT_KEYBOARD_CLASSIC
    ui_button(fb,12,147,222,23,"JOYSTICK SETUP",setup_focus==0?3:0);
    ui_button(fb,246,147,222,23,"KEYBOARD SETUP",setup_focus==1?3:0);
#endif
    box(fb,12,175,456,30,0,7);centred_text(fb,180,"8BITRETRO.LOCAL",false);centred_text(fb,195,load_error?load_error:"EMULATION PAUSED",false);
}
static void status_present(void) {
    const uint32_t *staged=(const uint32_t *)(board_psram()+board_psram_size()-BOARD_STATUS_BYTES);
    staged+=BANNER_ROWS*BOARD_DISPLAY_ROW_WORDS;
    uint32_t *bank=status_sram+status_active*STATUS_BANK_ROWS*BOARD_DISPLAY_ROW_WORDS;
    const uint16_t **table=runtime_rows+status_active*(BOARD_DISPLAY_HEIGHT+1);
    // Identical scanlines share immutable SRAM rows, including button fills
    // and borders, so two complete screens fit without large SRAM buffers.
    memset(bank,0,ROW_BYTES);
#if !MACHINE_C64 && !MACHINE_SPECTRUM
    bank[BOARD_DISPLAY_ROW_WORDS-1]=BOARD_DISPLAY_ROW_END;
#endif
    unsigned used=1;
    for(unsigned y=0;y<BANNER_ROWS;y++) {
        const uint32_t *src=staged+y*BOARD_DISPLAY_ROW_WORDS;
        bool nonblank=false;
        for(unsigned x=0;x<BOARD_DISPLAY_ROW_WORDS-1;x++)nonblank|=src[x]!=0;
        const uint32_t *row=bank;
        if(nonblank) {
            // Repeated button borders/fills share immutable rows, preserving SRAM budget.
            unsigned match=1;
            for(;match<used;match++)if(!memcmp(bank+match*BOARD_DISPLAY_ROW_WORDS,src,ROW_BYTES))break;
            if(match==used) {
                if(used>=STATUS_BANK_ROWS){status_active^=1u;return;}
                memcpy(bank+used++*BOARD_DISPLAY_ROW_WORDS,src,ROW_BYTES);
            }
            row=bank+match*BOARD_DISPLAY_ROW_WORDS;
        }
        table[y*2]=table[y*2+1]=(const uint16_t *)row;
    }
    table[BOARD_DISPLAY_HEIGHT]=NULL;
    __dmb();
    // Driver publishes the complete immutable bank at vertical blank only.
    board_display_set_rows(table);
}
static void status_close(void) {
    c64_runtime_diag_capture(C64_RUNTIME_DIAG_MENU_CLOSE_BEGIN, 0);
    setup_end();
#if NIMBUS_TEST && BT_KEYBOARD_CLASSIC
    touch_keyboard=false;
#endif
    titles_end();chooser=false;machine_chooser=false;touch_select=false;
#if NIMBUS_TEST
    controller_menu_capture(false);
#endif
#if BORROW_STATUS_FB
            board_display_backlight(0);
            memcpy(status_sram, board_psram()+board_psram_size()-BOARD_STATUS_BYTES, BANNER_ROWS*ROW_BYTES);
#endif
            board_display_set_rows(emulator_rows);
#if BORROW_STATUS_FB
            retire_display_table();
            machine_status_framebuffer(false);
            board_display_backlight(255);
#endif
#if WITH_SOUND
            audio_status_mute(false);
#endif
            status_visible = false;
    c64_runtime_diag_capture(C64_RUNTIME_DIAG_MENU_CLOSE_END, 0);

}
static bool status_open(void) {
    if(!status_allocate())return false;
    c64_runtime_diag_capture(C64_RUNTIME_DIAG_MENU_OPEN_BEGIN, 0);
#if NIMBUS_TEST
    controller_menu_capture(true);
#endif
            emulator_rows = board_display_get_rows();
#if WITH_SOUND
            audio_status_mute(true);
#endif
#if BORROW_STATUS_FB
            board_display_backlight(0);
            status_sram = machine_status_framebuffer(true);
            memcpy(board_psram()+board_psram_size()-BOARD_STATUS_BYTES, status_sram, BANNER_ROWS*ROW_BYTES);
#endif
            status_active ^= 1u;
            status_draw(status_active);
            status_present();
#if BORROW_STATUS_FB
            retire_display_table();board_display_backlight(255);
#endif
            status_visible = true;
    c64_runtime_diag_capture(C64_RUNTIME_DIAG_MENU_OPEN_END, 0);

    return true;
}
#endif

void boot_status_frame(void) {
#if BOOT_STATUS_TRIAL
    static bool gpio_ready;
    static boot_button_t button;
    static bool pending_toggle;
    if (!gpio_ready) {
        // Pimoroni Presto schematic: USER_SW on GP46, active low. This reads
        // the dedicated input; it does not manipulate QSPI chip select.
        gpio_init(BOARD_BOOT_BUTTON_PIN);
        gpio_set_dir(BOARD_BOOT_BUTTON_PIN, GPIO_IN);
        gpio_pull_up(BOARD_BOOT_BUTTON_PIN);
        gpio_ready = true;
    }
    uint32_t now = to_ms_since_boot(get_absolute_time());
    if (boot_button_press(&button, !gpio_get(BOARD_BOOT_BUTTON_PIN), now))
        pending_toggle = !pending_toggle;
    uint32_t frame = board_display_frames();
    // Allow the previous scanout table/buffer to retire without blocking audio.
    if ((uint32_t)(frame - status_frame) < 2) return;
    static bool touch_held;
    unsigned tx=0,ty=0;bool touched=board_touch(&tx,&ty);
    bool tap=touched&&!touch_held;touch_held=touched;ty/=2;
    // A touch opens the keyboard; a physical BOOT press keeps the file-first path.
    bool open_keyboard=tap&&!status_visible&&!pending_toggle;
    if(tap&&!status_visible)pending_toggle=true;
#if NIMBUS_TEST && BT_KEYBOARD_CLASSIC
    else if(status_visible&&touch_keyboard) {
        if(pending_toggle){pending_toggle=false;touch_keyboard=false;fire_armed=false;chooser_dirty=true;}
        else touch_key_frame(tap,tx,ty,now);
    }
    else if(status_visible&&setup_page) {
        if(pending_toggle){pending_toggle=false;setup_end();}
        else setup_frame(tap,touched,tx,ty,now);
    }
    else if(status_visible&&!chooser&&!machine_chooser&&tap&&ty>=147&&ty<170) {
        setup_open(tx>=246);
    }
#endif
    else if(status_visible&&touched) {
        if(tap&&ty>=214) {
            if(tx>=320){status_close();return;}
            if(tx>=160)pending_toggle=true;
            else if(machine_chooser){titles_begin();chooser_dirty=true;}
            else if(!chooser){machines_begin();}
            else {
#if NIMBUS_TEST && BT_KEYBOARD_CLASSIC
                touch_key_open();
#else
                status_close();return;
#endif
            }
        } else if(machine_chooser) {
            if(tap&&ty>=28&&ty<92){machine_selected=((ty-28)/34)*2+(tx>=246);if(machine_selected>3)machine_selected=3;fire_armed=true;touch_select=true;}
            else if((ty>=104&&ty<151)||(ty>=154&&ty<205)) {
                bool on,led,locks;unsigned vol,bright;char args[32];
                audio_get(&on,&vol);board_leds_get(&led,&bright,&locks);
                bool sound=ty<151;
                if(((sound&&ty<128)||(!sound&&ty<182)) && tx<182) {
                    if(tap){snprintf(args,sizeof args,"%s",sound?(on?"off":"on"):(led?"off":"screen"));api_req_t r={.args=args};api_call(sound?"sound":"ambient",&r);chooser_dirty=true;}
                } else if((sound&&ty>=130)||(!sound&&ty>=184)) {
                    unsigned value=tx<=20?0:tx>=460?100:(tx-20)*100/440;
                    if(value!=(sound?vol:bright)){snprintf(args,sizeof args,"%s %u",sound?"vol":"bright",value);api_req_t r={.args=args};api_call(sound?"sound":"ambient",&r);chooser_dirty=true;}
                }
            }
        } else if(tap&&chooser&&!title_iterator&&ty>=183&&ty<207) {
            if(tx<160&&title_selected)title_selected--;
            else if(tx>=160&&tx<320&&title_selected+1<title_count)title_selected++;
            else if(tx>=320&&title_count){fire_armed=true;touch_select=true;}
            chooser_dirty=true;
        } else if(tap&&chooser&&!title_iterator&&tx>=12&&tx<468&&ty>=35&&ty<160&&(ty-35)%25<22) {
            unsigned selected=(title_selected/5)*5+(ty-35)/25;
            if(selected<title_count){title_selected=selected;chooser_dirty=true;fire_armed=true;touch_select=true;}
        }
    }
    if (pending_toggle) {
        pending_toggle = false;
        if(status_visible && chooser
#if NIMBUS_TEST && BT_KEYBOARD_CLASSIC
       && !touch_keyboard
#endif
    ) {
            machines_begin();
        } else if(status_visible && machine_chooser) {
            machine_chooser=false;chooser_dirty=true;
#if NIMBUS_TEST && BT_KEYBOARD_CLASSIC
            setup_focus=0;setup_fire_armed=false;setup_direction=0;
#endif
        } else if(status_visible)status_close();
        else {
            titles_begin();
            if(!status_open())titles_end();
#if NIMBUS_TEST && BT_KEYBOARD_CLASSIC
            else if(open_keyboard)touch_key_open();
#else
            (void)open_keyboard;
#endif
        }
        status_frame = frame; status_updated_ms = now;
    }
    if(status_visible && machine_chooser) {
        unsigned directions=0;bool fire=false;
        joystick_state(&directions,&fire);fire|=touch_select;touch_select=false;
        if(!fire)fire_armed=true;
        int direction=(directions&1)?-1:((directions&2)?1:0);
        if(direction && (direction!=held_direction || (int32_t)(now-repeat_at)>=0)) {
            if(direction<0 && machine_selected)machine_selected--;
            if(direction>0 && machine_selected<3)machine_selected++;
            repeat_at=now+(direction!=held_direction?400:140);chooser_dirty=true;
        }
        held_direction=direction;
        if(fire && fire_armed) {
            const char *want=machine_ids[machine_selected];
            status_close();
            // Reuse the web/USB switch path: flush preferences and disc saves,
            // persist the selection, then restart through the launcher.
            api_req_t request={.args=want,.from="BOOT machine selector",.json=false};
            if(!api_call("machine",&request) || request.code!=200) {
                load_error="MACHINE SWITCH FAILED";status_open();
            }
            status_frame=board_display_frames();status_updated_ms=now;
            return;
        }
    }
    if(status_visible && chooser
#if NIMBUS_TEST && BT_KEYBOARD_CLASSIC
       && !touch_keyboard
#endif
    ) {
        titles_poll();
        unsigned directions=0;bool fire=false;
        joystick_state(&directions,&fire);fire|=touch_select;touch_select=false;
        if(!title_iterator && title_count) {
            if(!fire)fire_armed=true;
            int direction=(directions&1)?-1:((directions&2)?1:0);
            if(direction && (direction!=held_direction || (int32_t)(now-repeat_at)>=0)) {
                if(direction<0 && title_selected)title_selected--;
                if(direction>0 && title_selected+1<title_count)title_selected++;
                repeat_at=now+(direction!=held_direction?400:140);chooser_dirty=true;
            }
            held_direction=direction;
            if(fire && fire_armed) {
                char name[TITLE_NAME_BYTES];snprintf(name,sizeof name,"%s",titles()[title_selected]);
                fire_armed=false;status_close();
                if(!board_sd_load_disc(name)) {
                    load_error="LOAD FAILED - CHECK SD CARD";chooser=false;status_open();
                }
                status_frame=board_display_frames();status_updated_ms=now;
                return;
            }
        }
    }
#if NIMBUS_TEST && BT_KEYBOARD_CLASSIC
    if(status_visible&&!chooser&&!machine_chooser&&!setup_page) {
        unsigned directions=0;bool fire=false;joystick_state(&directions,&fire);
        if(!fire)setup_fire_armed=true;
        int direction=(directions&1)?-1:(directions&2)?1:0;
        if(direction&&direction!=setup_direction){setup_focus=direction>0?1:0;chooser_dirty=true;}
        setup_direction=direction;
        if(fire&&setup_fire_armed){setup_fire_armed=false;setup_open(setup_focus==1);}
    }
#endif
    if (status_visible && (chooser_dirty || (!chooser && (uint32_t)(now - status_updated_ms) >= 1000))) {
        chooser_dirty=false;
        status_active ^= 1u;
        status_draw(status_active);
        status_present();
        status_frame = frame; status_updated_ms = now;
    }
#endif
}

// All runtime status scanout is now SRAM, including BBC packed-4 rows.
void boot_status_flash_begin(void) {}
void boot_status_flash_end(void) {}

bool boot_status_visible(void) {
#if BOOT_STATUS_TRIAL
    return status_visible;
#else
    return false;
#endif
}
