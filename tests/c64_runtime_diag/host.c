#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mock.h"
#include "c64_runtime_diag.h"
// The runner supplies the actual collector implementation in this translation unit.
#include "runtime_diag_subject.c"

static uint16_t *valid_devinfo;
static void seed(unsigned value) {
    for(unsigned m=0;m<2;m++) {
        mock_qmi.m[m].timing=0xf0000000u + value*256 + m*16;
        mock_qmi.m[m].rfmt=0xf0000001u + value*256 + m*16;
        mock_qmi.m[m].rcmd=0xf0000002u + value*256 + m*16;
        mock_qmi.m[m].wfmt=0xf0000003u + value*256 + m*16;
        mock_qmi.m[m].wcmd=0xf0000004u + value*256 + m*16;
    }
    mock_xip.ctrl=0xe0000000u+value;
    mock_xip.ctr_hit=0x12345678;mock_xip.ctr_acc=0x87654321;
    mock_timer.timerawh=0x80000000u+value;
    mock_timer.timerawl=0xffff0000u+value;
    for(unsigned i=0;i<16;i++)mock_bootram[i]=0xd0000000u+value*256+i;
    mock_clocks.clk[clk_sys].ctrl=0xc0000000u+value;
    mock_clocks.clk[clk_sys].div=0xc1000000u+value;
    mock_pll.prim=0xc2000000u+value;
    mock_pll.fbdiv_int=0xc3000000u+value;
    if(valid_devinfo)*valid_devinfo=(uint16_t)(0xa000u+value);
}
static void max_values(void) {
    memset(&mock_qmi,0xff,sizeof mock_qmi);memset(&mock_xip,0xff,sizeof mock_xip);
    memset(&mock_timer,0xff,sizeof mock_timer);memset(&mock_clocks,0xff,sizeof mock_clocks);
    memset(&mock_pll,0xff,sizeof mock_pll);memset(mock_bootram,0xff,sizeof mock_bootram);
    memset(mock_sram,0xff,sizeof mock_sram);
}

// Compare every mocked register byte, including cache counters not collected.
static unsigned char hardware_before[sizeof mock_qmi+sizeof mock_xip+sizeof mock_timer+
    sizeof mock_clocks+sizeof mock_pll+sizeof mock_bootram+sizeof mock_sram];
static void hardware_copy(unsigned char *out) {
#define COPY_HW(object) do { memcpy(out,&object,sizeof object);out+=sizeof object; } while(0)
    COPY_HW(mock_qmi);COPY_HW(mock_xip);COPY_HW(mock_timer);COPY_HW(mock_clocks);
    COPY_HW(mock_pll);COPY_HW(mock_bootram);COPY_HW(mock_sram);
#undef COPY_HW
}
static void hardware_check(void) {
    unsigned char after[sizeof hardware_before];hardware_copy(after);
    assert(!memcmp(hardware_before,after,sizeof after));
}
static void capture(c64_runtime_diag_event_t event,uint32_t arg) {
    unsigned previous_lookups=lookup_calls,previous_clocks=clock_calls;
    unsigned previous_lcd=lcd_calls,previous_flash=flash_calls;
    hardware_copy(hardware_before);in_capture=true;
    c64_runtime_diag_capture(event,arg);
    in_capture=false;hardware_check();
    assert(previous_lookups==lookup_calls && previous_clocks==clock_calls);
    assert(previous_lcd==lcd_calls && previous_flash==flash_calls);
}
static void emit(const char *label,bool summary) {
    char out[C64_RUNTIME_DIAG_JSON_CAPACITY];hardware_copy(hardware_before);
    if(summary)c64_runtime_diag_summary_json(out,sizeof out);
    else c64_runtime_diag_json(out,sizeof out);
    hardware_check();
    printf("{\"label\":\"%s\",\"kind\":\"%s\",\"value\":",label,summary?"summary":"full");
    if(summary)printf("{\"anchor\":true%s}",out);else fputs(out,stdout);
    puts("}");
}

static void bounded_output(bool summary) {
    char previous[C64_RUNTIME_DIAG_JSON_CAPACITY*2];previous[0]=0;
    bool have_previous=false;
    for(size_t cap=0;cap<=C64_RUNTIME_DIAG_JSON_CAPACITY+16;cap++) {
        size_t total=cap+32;
        unsigned char *storage=malloc(total);assert(storage);
        memset(storage,0xa5,total);char *out=(char *)storage+16;
        hardware_copy(hardware_before);
        if(summary)c64_runtime_diag_summary_json(out,cap);else c64_runtime_diag_json(out,cap);
        hardware_check();
        for(unsigned i=0;i<16;i++)assert(storage[i]==0xa5);
        for(size_t i=16+cap;i<total;i++)assert(storage[i]==0xa5);
        if(!summary && cap>C64_RUNTIME_DIAG_JSON_CAPACITY)
            for(size_t i=16+C64_RUNTIME_DIAG_JSON_CAPACITY;i<16+cap;i++)assert(storage[i]==0xa5);
        if(cap) {
            assert(memchr(out,0,cap));
            if(!summary && cap>=3)assert(out[0]);
            if(!have_previous || strcmp(previous,out)) {
                printf("{\"label\":\"bounded\",\"kind\":\"%s\",\"cap\":%zu,\"value\":",
                       summary?"summary":"full",cap);
                if(summary)printf("{\"anchor\":true%s}",out);
                else if(out[0])fputs(out,stdout);else fputs("null",stdout);
                puts("}");strcpy(previous,out);have_previous=true;
            }
        }
        free(storage);
    }
    // Null output pointers must be accepted at all representative capacities.
    c64_runtime_diag_json(NULL,0);c64_runtime_diag_json(NULL,1);
    c64_runtime_diag_json(NULL,C64_RUNTIME_DIAG_JSON_CAPACITY);
    c64_runtime_diag_summary_json(NULL,0);c64_runtime_diag_summary_json(NULL,1);
    c64_runtime_diag_summary_json(NULL,C64_RUNTIME_DIAG_JSON_CAPACITY);
}

int main(int argc,char **argv) {
    assert(argc==2);
    if(!strcmp(argv[1],"normal") || !strcmp(argv[1],"sram") || !strcmp(argv[1],"max"))valid_devinfo=&mock_sram[3];
    else if(!strcmp(argv[1],"bootram"))valid_devinfo=(uint16_t *)&mock_bootram[10];
    else if(!strcmp(argv[1],"sram_last"))valid_devinfo=(uint16_t *)(SRAM_END-2);
    else if(!strcmp(argv[1],"bootram_last"))valid_devinfo=(uint16_t *)(BOOTRAM_END-2);
    mock_devinfo_literal=valid_devinfo;
    if(!strcmp(argv[1],"missing_literal"))mock_missing_literal=true;
    if(!strcmp(argv[1],"odd"))mock_devinfo_literal=(uint16_t *)(SRAM_BASE+1);
    if(!strcmp(argv[1],"sram_past"))mock_devinfo_literal=(uint16_t *)SRAM_END;
    if(!strcmp(argv[1],"bootram_past"))mock_devinfo_literal=(uint16_t *)BOOTRAM_END;
    if(!strcmp(argv[1],"outside"))mock_devinfo_literal=(uint16_t *)(uintptr_t)0x10000000;
    seed(1);
    if(!strcmp(argv[1],"max"))max_values();
    capture(C64_RUNTIME_DIAG_BOOT_READY,99);emit("before_init",false);
    assert(lookup_calls==0);
    c64_runtime_diag_init();assert(lookup_calls==1);
    emit("after_init",false);emit("after_init",true);
    c64_runtime_diag_init();assert(lookup_calls==1);
    capture((c64_runtime_diag_event_t)0,77);capture((c64_runtime_diag_event_t)8,77);
    capture((c64_runtime_diag_event_t)UINT32_MAX,77);
    capture(C64_RUNTIME_DIAG_BOOT_READY,!strcmp(argv[1],"max")?UINT32_MAX:0x1111);
    emit("baseline",false);emit("baseline",true);
    if(!strcmp(argv[1],"max")) {
        for(unsigned i=0;i<8;i++)capture(C64_RUNTIME_DIAG_FLASH_END,UINT32_MAX);
        emit("maximum",false);emit("maximum",true);
        bounded_output(false);bounded_output(true);
        return 0;
    }
    if(strcmp(argv[1],"normal"))return 0;

    // Each changed-mask group is independent; timestamps are deliberately ignored.
    mock_timer.timerawh++;emit("mask_time_only",true);seed(1);
    mock_qmi.m[0].timing^=1;emit("mask_m0",true);seed(1);
    mock_qmi.m[1].wcmd^=1;emit("mask_m1",true);seed(1);
    mock_xip.ctrl^=1;emit("mask_xip",true);seed(1);
    mock_bootram[4]^=1;emit("mask_bootram",true);seed(1);
    mock_pll.fbdiv_int^=1;emit("mask_clocks",true);seed(1);
    *valid_devinfo^=1;emit("mask_devinfo",true);seed(1);
    seed(2);emit("mask_all",true);

    for(unsigned i=1;i<=25;i++) {
        seed(i+1);
        c64_runtime_diag_event_t event=(c64_runtime_diag_event_t)(2+(i-1)%6);
        if(i==13)event=C64_RUNTIME_DIAG_BOOT_READY; // Must not replace baseline.
        capture(event,0x10000000u+i);
    }
    emit("wrapped",false);emit("wrapped",true);
    c64_runtime_diag_init();assert(lookup_calls==1);
    emit("reinitialized",false);
    bounded_output(false);bounded_output(true);
    emit("after_serialization",false);

    // Force only counters near their documented limits; avoid billions of events.
    sequence=UINT32_MAX-1;overwritten=UINT32_MAX-1;
    capture(C64_RUNTIME_DIAG_FLASH_BEGIN,0xfffff000u);
    capture(C64_RUNTIME_DIAG_FLASH_END,0xfffff000u);
    emit("counter_wrap",false);emit("counter_wrap",true);
    return 0;
}
