#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "mock.h"
#include "mem_subject.c"

enum action { BOOT_BEGIN=1, LOCK_QUERY, IRQ_SAVE, SAFE_ENTER, DIAG_BEGIN,
              ERASE, PROGRAM, DSB, ISB, DIAG_END, IRQ_RESTORE, SAFE_LEAVE, BOOT_END };
static unsigned actions[32],action_count,current_core,queried_core;
static bool victim_ready,irq_disabled,initial_irq_disabled,in_safe,call_callback;
static int safe_result;
static uint32_t irq_token;
static mock_window original[2],diagnostic[2][2];
static uint32_t diag_arg[2];
static unsigned diag_count,erase_count,program_count;
static struct {uint32_t offset;size_t count;} programs[3];
#define FLASH_BASE 0xf00000u
static uint8_t flash_bytes[FLASH_SECTOR_SIZE*3];
static uint8_t input[FLASH_SECTOR_SIZE];
static uint32_t target_offset;
static void record(unsigned action) {assert(action_count<32);actions[action_count++]=action;}
static bool equal_window(const mock_window *a,const mock_window *b) {
    return a->timing==b->timing && a->rfmt==b->rfmt && a->rcmd==b->rcmd &&
           a->wfmt==b->wfmt && a->wcmd==b->wcmd;
}
static void assert_critical(void) {
    assert(irq_disabled && (in_safe || !victim_ready));
}
static void sdk_downgrade(void) {
    // Every SDK erase/program call re-enters generic serial-03h read mode.
    // These RFMT/RCMD values reproduce the captured hardware transition.
    mock_qmi.m[0].timing=0x40000004;
    mock_qmi.m[0].rfmt=0x1000;
    mock_qmi.m[0].rcmd=0x03;
}
unsigned get_core_num(void) {return current_core;}
bool multicore_lockout_victim_is_initialized(unsigned core) {
    record(LOCK_QUERY);queried_core=core;return victim_ready;
}
uint32_t save_and_disable_interrupts(void) {
    record(IRQ_SAVE);assert(!victim_ready);
    irq_token=0x12345600u+(irq_disabled?1:0);irq_disabled=true;return irq_token;
}
void restore_interrupts(uint32_t state) {
    record(IRQ_RESTORE);assert(!victim_ready && state==irq_token);
    assert(irq_disabled);irq_disabled=(state&1)!=0;
}
int flash_safe_execute(void (*callback)(void *),void *arg,uint32_t timeout) {
    record(SAFE_ENTER);assert(victim_ready && callback==write_sector && !arg && timeout==1000);
    assert(!in_safe);bool previous=irq_disabled;in_safe=true;irq_disabled=true;
    if(call_callback)callback(arg);
    irq_disabled=previous;in_safe=false;record(SAFE_LEAVE);return safe_result;
}
void flash_range_erase(uint32_t offset,size_t count) {
    assert_critical();record(ERASE);erase_count++;
    assert(offset==target_offset && offset%FLASH_SECTOR_SIZE==0 && count==FLASH_SECTOR_SIZE);
    assert(offset>=FLASH_BASE && offset+count<=FLASH_BASE+sizeof flash_bytes);
    memset(flash_bytes+(offset-FLASH_BASE),0xff,count);sdk_downgrade();
}
void flash_range_program(uint32_t offset,const uint8_t *data,size_t count) {
    assert_critical();record(PROGRAM);assert(program_count<3);
    assert(offset%FLASH_PAGE_SIZE==0 && count && count%FLASH_PAGE_SIZE==0);
    assert(offset>=target_offset && offset+count<=target_offset+FLASH_SECTOR_SIZE);
    programs[program_count].offset=offset;programs[program_count++].count=count;
    memcpy(flash_bytes+(offset-FLASH_BASE),data,count);sdk_downgrade();
}
void boot_status_flash_begin(void) {
    record(BOOT_BEGIN);assert(!in_safe && irq_disabled==initial_irq_disabled);
}
void boot_status_flash_end(void) {
    record(BOOT_END);assert(!in_safe && irq_disabled==initial_irq_disabled);
}
const char *machine_name(void) {return "c64";}
bool system_launched(void) {return true;}
void __dsb(void) {
    assert_critical();record(DSB);
}
void __isb(void) {
    assert_critical();assert(action_count && actions[action_count-1]==DSB);
    assert(equal_window(&mock_qmi.m[0],&original[0]));record(ISB);
}
void c64_runtime_diag_capture(c64_runtime_diag_event_t event,uint32_t arg) {
    assert_critical();assert(diag_count<2);
    assert(event==(diag_count?C64_RUNTIME_DIAG_FLASH_END:C64_RUNTIME_DIAG_FLASH_BEGIN));
    record(diag_count?DIAG_END:DIAG_BEGIN);
    diag_arg[diag_count]=arg;
    for(unsigned m=0;m<2;m++)diagnostic[diag_count][m]=mock_qmi.m[m];
    diag_count++;
}

static void reset_case(bool quad,bool ready,bool disabled,unsigned core) {
    action_count=diag_count=erase_count=program_count=0;
    victim_ready=ready;initial_irq_disabled=irq_disabled=disabled;
    current_core=core;queried_core=99;in_safe=false;call_callback=true;safe_result=PICO_OK;
    target_offset=FLASH_BASE+FLASH_SECTOR_SIZE;
    mock_qmi.m[0]=(mock_window){0x60007203u,quad?0x492a8u:0x1000u,quad?0xebu:0x03u,0x1000u,0xa002u};
    mock_qmi.m[1]=(mock_window){0x603e4302u,0x612aau,0xebu,0x12aau,0x38u};
    original[0]=mock_qmi.m[0];original[1]=mock_qmi.m[1];
    memset(flash_bytes,0x5a,sizeof flash_bytes);
    for(unsigned i=0;i<sizeof input;i++)input[i]=(uint8_t)(i*17u+i/256u);
}
static void check_trace(bool callback_ran) {
    unsigned expected[32],n=0;
    expected[n++]=BOOT_BEGIN;expected[n++]=LOCK_QUERY;
    expected[n++]=victim_ready?SAFE_ENTER:IRQ_SAVE;
    if(callback_ran) {
        expected[n++]=DIAG_BEGIN;expected[n++]=ERASE;
        for(unsigned p=0;p<program_count;p++)expected[n++]=PROGRAM;
        expected[n++]=DSB;expected[n++]=DSB;expected[n++]=ISB;expected[n++]=DIAG_END;
    }
    expected[n++]=victim_ready?SAFE_LEAVE:IRQ_RESTORE;expected[n++]=BOOT_END;
    assert(action_count==n);
    for(unsigned i=0;i<n;i++)assert(actions[i]==expected[i]);
    assert(queried_core==(current_core^1u));
    assert(!in_safe && irq_disabled==initial_irq_disabled);
}
static void check_write(const uint8_t *data,uint32_t len,bool callback_ran) {
    assert(equal_window(&mock_qmi.m[0],&original[0]));
    assert(equal_window(&mock_qmi.m[1],&original[1]));
    check_trace(callback_ran);
    if(!callback_ran) {
        assert(!diag_count && !erase_count && !program_count);
        for(unsigned i=0;i<sizeof flash_bytes;i++)assert(flash_bytes[i]==0x5a);
        return;
    }
    assert(diag_count==2 && erase_count==1);
    for(unsigned i=0;i<2;i++) {
        assert(diag_arg[i]==target_offset);
        for(unsigned m=0;m<2;m++)assert(equal_window(&diagnostic[i][m],&original[m]));
    }
    uint32_t payload=data?len:0,whole=payload&~(FLASH_PAGE_SIZE-1);
    assert(program_count==(unsigned)(whole!=0)+(unsigned)(payload!=whole));
    unsigned p=0;
    if(whole){assert(programs[p].offset==target_offset && programs[p].count==whole);p++;}
    if(payload!=whole)assert(programs[p].offset==target_offset+whole && programs[p].count==FLASH_PAGE_SIZE);
    for(unsigned i=0;i<sizeof flash_bytes;i++) {
        uint8_t want=0x5a;
        if(i>=FLASH_SECTOR_SIZE && i<2*FLASH_SECTOR_SIZE) {
            unsigned relative=i-FLASH_SECTOR_SIZE;
            want=relative<payload?input[relative]:0xff;
        }
        assert(flash_bytes[i]==want);
    }
    for(unsigned i=0;i<sizeof input;i++)assert(input[i]==(uint8_t)(i*17u+i/256u));
}
static void print_window(const mock_window *w) {
    printf("{\"timing\":\"%08x\",\"rfmt\":\"%08x\",\"rcmd\":\"%08x\"}",
           w->timing,w->rfmt,w->rcmd);
}
static int observed_probe(void) {
    reset_case(true,true,false,0);
    assert(mem_flash_write_sector(target_offset,input,257));
    bool restored=equal_window(&original[0],&mock_qmi.m[0]);
    printf("{\"case\":\"captured_quad_read_mode\",\"before\":");print_window(&original[0]);
    printf(",\"after\":");print_window(&mock_qmi.m[0]);
    printf(",\"diagnostic_after\":");print_window(&diagnostic[1][0]);
    printf(",\"timing_restored\":%s,\"read_mode_restored\":%s,\"m1_unchanged\":%s}\n",
           mock_qmi.m[0].timing==original[0].timing?"true":"false",restored?"true":"false",
           equal_window(&original[1],&mock_qmi.m[1])?"true":"false");
    puts(restored?"PASS: original M0 timing, command and format restored":"FAIL: SDK serial-read downgrade survived the save callback");
    return restored?0:1;
}
int main(int argc,char **argv) {
    if(argc==2 && !strcmp(argv[1],"--probe"))return observed_probe();
    assert(argc==1);unsigned cases=0;
    const unsigned lengths[]={0,1,255,256,257,512,1024,4095,4096};
    for(unsigned quad=0;quad<2;quad++)for(unsigned ready=0;ready<2;ready++)
    for(unsigned disabled=0;disabled<2;disabled++)for(unsigned core=0;core<2;core++) {
        for(unsigned i=0;i<sizeof lengths/sizeof lengths[0];i++) {
            reset_case(quad,ready,disabled,core);uint32_t writes=mem_flash_writes();
            assert(mem_flash_write_sector(target_offset,input,lengths[i]));
            assert(mem_flash_writes()==writes+1);check_write(input,lengths[i],true);cases++;
        }
        // NULL explicitly requests erasure, even when an otherwise-valid length is supplied.
        for(unsigned length=0;length<=FLASH_SECTOR_SIZE;length+=FLASH_SECTOR_SIZE) {
            reset_case(quad,ready,disabled,core);assert(mem_flash_write_sector(target_offset,NULL,length));
            check_write(NULL,length,true);cases++;
        }
        // Invalid requests must not enter hooks/lockout, touch registers, or count attempts.
        reset_case(quad,ready,disabled,core);uint32_t writes=mem_flash_writes();
        assert(!mem_flash_write_sector(target_offset+1,input,0));
        assert(!mem_flash_write_sector(target_offset,input,FLASH_SECTOR_SIZE+1));
        assert(!mem_flash_write_sector(target_offset,NULL,FLASH_SECTOR_SIZE+1));
        assert(!action_count && !diag_count && !erase_count && !program_count);
        assert(mem_flash_writes()==writes && irq_disabled==initial_irq_disabled);
        assert(equal_window(&mock_qmi.m[0],&original[0]) && equal_window(&mock_qmi.m[1],&original[1]));
        for(unsigned i=0;i<sizeof flash_bytes;i++)assert(flash_bytes[i]==0x5a);
        cases+=3;
    }
    for(unsigned ran=0;ran<2;ran++) {
        reset_case(true,true,false,0);call_callback=ran;safe_result=-7;
        uint32_t writes=mem_flash_writes();assert(!mem_flash_write_sector(target_offset,input,257));
        assert(mem_flash_writes()==writes+1);check_write(input,257,ran);cases++;
    }
    // Repeated preference saves must not adopt the SDK's degraded mode as the new baseline.
    reset_case(true,true,false,0);uint32_t writes=mem_flash_writes();
    for(unsigned i=0;i<20;i++) {
        action_count=diag_count=erase_count=program_count=0;
        memset(flash_bytes,0x5a,sizeof flash_bytes);
        assert(mem_flash_write_sector(target_offset,input,i%2?1:FLASH_SECTOR_SIZE));
        check_write(input,i%2?1:FLASH_SECTOR_SIZE,true);cases++;
    }
    assert(mem_flash_writes()==writes+20);
    printf("PASS: %u cases; erase/page/tail/full-sector data, repeated saves, serial/quad preservation, "
           "M1 unchanged, diagnostic/barrier order, lockout/IRQ/error paths and invalid no-effects\n",cases);
    return 0;
}
