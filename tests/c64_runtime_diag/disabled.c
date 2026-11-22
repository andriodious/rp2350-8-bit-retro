#include <assert.h>
#include <stddef.h>
#include <string.h>
#include "c64_runtime_diag.h"

int main(void) {
    c64_runtime_diag_init();
    c64_runtime_diag_capture(C64_RUNTIME_DIAG_BOOT_READY,0);
    c64_runtime_diag_capture(C64_RUNTIME_DIAG_FLASH_END,0xffffffffu);
    for(size_t cap=0;cap<=16;cap++) {
        char bytes[18];memset(bytes,'X',sizeof bytes);
        c64_runtime_diag_json(bytes+1,cap);
        assert(bytes[0]=='X');for(size_t i=1+cap;i<sizeof bytes;i++)assert(bytes[i]=='X');
        if(cap>=3)assert(!strcmp(bytes+1,"{}"));else if(cap)assert(bytes[1]==0);
        memset(bytes,'X',sizeof bytes);
        c64_runtime_diag_summary_json(bytes+1,cap);
        assert(bytes[0]=='X');for(size_t i=1+cap;i<sizeof bytes;i++)assert(bytes[i]=='X');
        if(cap)assert(bytes[1]==0);
    }
    c64_runtime_diag_json(NULL,0);c64_runtime_diag_json(NULL,16);
    c64_runtime_diag_summary_json(NULL,0);c64_runtime_diag_summary_json(NULL,16);
    return 0;
}
