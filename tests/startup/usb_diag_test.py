"""Check wrapper arguments, nested marker order, and false-return propagation."""
from pathlib import Path
import subprocess
import tempfile
root = Path(__file__).resolve().parents[2]
source = '\n'.join(line for line in (root/'src/boards/presto/startup_usb_diag.c').read_text().splitlines() if not line.startswith('#include'))
stub = r'''
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>
typedef struct {int value;} tusb_rhport_init_t;
static unsigned marks[8], count;
static bool result;
static tusb_rhport_init_t config={7};
void startup_diag_mark(unsigned stage){assert(count<8);marks[count++]=stage;}
'''
main = r'''
bool __real_stdio_usb_init(void){return __wrap_tusb_rhport_init(0,&config);}
bool __real_tusb_rhport_init(uint8_t port,const tusb_rhport_init_t *p){assert(port==0&&p==&config);return __wrap_dcd_init(port,p);}
bool __real_dcd_init(uint8_t port,const tusb_rhport_init_t *p){assert(port==0&&p==&config);__wrap_rp2040_usb_init();return result;}
void __real_rp2040_usb_init(void){}
int main(void){
 const unsigned expected[]={0x301,0x302,0x303,0x304,0x305,0x306,0x307,0x401};
 for(int r=0;r<2;r++){
  count=0;result=r;assert(__wrap_stdio_usb_init()==result);assert(count==8);
  for(int i=0;i<8;i++)assert(marks[i]==expected[i]);
 }
}
'''
with tempfile.TemporaryDirectory() as t:
    p=Path(t); (p/'test.c').write_text(stub+source+main)
    subprocess.run(['cc','-std=c11','-fsanitize=address,undefined',str(p/'test.c'),'-o',str(p/'test')],check=True)
    subprocess.run([str(p/'test')],check=True)
print('USB wrapper marker order, arguments and return values passed')
