"""Host-check the actual startup LED sender's ownership and fallback paths."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[2]
source=(root/'src/boards/presto/startup_diag.c').read_text()
source='\n'.join(x for x in source.splitlines() if not x.startswith('#include'))
stub=r'''
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>
typedef unsigned uint;
typedef int PIO;
typedef int pio_sm_config;
#define pio0 0
#define PICO_OK 0
#define PIO_FIFO_JOIN_TX 0
#define clk_sys 0
static int ws2812_program;
static int base_result,claim_result,can_add=1,claimed,released,removed,writes,enabled,delayed;
static uint32_t words[7];
static int pio_set_gpio_base(PIO p,int b){assert(p==0&&b==16);return base_result;}
static int pio_claim_unused_sm(PIO p,bool required){assert(!required);claimed++;return claim_result;}
static bool pio_can_add_program(PIO p,void *x){return can_add;}
static void pio_sm_unclaim(PIO p,int s){released++;}
static uint pio_add_program(PIO p,void *x){return 0;}
static void pio_gpio_init(PIO p,int pin){assert(pin==33);}
static void pio_sm_set_consecutive_pindirs(PIO p,int s,int pin,int n,bool out){assert(pin==33&&n==1&&out);}
static pio_sm_config ws2812_program_get_default_config(uint n){return 0;}
static void sm_config_set_sideset_pins(pio_sm_config *c,int pin){assert(pin==33);}
static void sm_config_set_out_shift(pio_sm_config *c,bool right,bool pull,int bits){assert(!right&&pull&&bits==24);}
static void sm_config_set_fifo_join(pio_sm_config *c,int j){}
static void sm_config_set_clkdiv(pio_sm_config *c,float f){assert(f==31.25f);}
static unsigned clock_get_hz(int clk){return 250000000;}
static void pio_sm_init(PIO p,int s,int offset,pio_sm_config *c){}
static void pio_sm_set_enabled(PIO p,int s,bool on){enabled+=on?1:-1;}
static void pio_sm_put_blocking(PIO p,int s,uint32_t w){assert(writes<7);words[writes++]=w;}
static void busy_wait_us_32(unsigned us){assert(us==600);delayed++;}
static void pio_remove_program(PIO p,void *x,int offset){removed++;}
'''
main=r'''
int main(void){
 assert(startup_diag_pixel(0x101,0)==0x00000800u);
 assert(startup_diag_pixel(0x201,0)==0x04080000u);
 assert(startup_diag_pixel(0x105,5)==0);
 assert(startup_diag_pixel(0x301,0)==0x00080000u);
 assert(startup_diag_pixel(0x401,0)==0x08080800u);
 startup_diag_mark(3);
 assert(writes==7&&released==1&&removed==1&&enabled==0&&delayed==1);
 for(int i=0;i<7;i++)assert(words[i]==(i<3?0x08000000u:0));
 can_add=0;startup_diag_mark(4);assert(released==2&&removed==1&&writes==7);
 claim_result=-1;startup_diag_mark(4);assert(released==2);
 base_result=-1;int old=claimed;startup_diag_mark(4);assert(claimed==old);
 return 0;
}
'''
with tempfile.TemporaryDirectory() as t:
 p=Path(t);(p/'test.c').write_text(stub+'\n#define STARTUP_DIAG 1\n'+(root/'src/common/startup_diag.h').read_text().replace('#pragma once','')+source+main)
 subprocess.run(['cc','-std=c11','-fsanitize=address,undefined',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
 (p/'override.c').write_text('#include <stdbool.h>\nbool startup_diag_ambient(unsigned stage){return stage==6;}\n')
 (p/'test.c').write_text(stub+'\n#define STARTUP_DIAG 1\n'+(root/'src/common/startup_diag.h').read_text().replace('#pragma once','')+source+'\nint main(void){startup_diag_mark(6);assert(claimed==0&&writes==0);return 0;}\n')
 subprocess.run(['cc','-std=c11','-fsanitize=address,undefined',str(p/'test.c'),str(p/'override.c'),'-o',str(p/'override')],check=True)
 subprocess.run([str(p/'override')],check=True)
print('Startup LED count, PIO release and unavailable-resource paths passed')
