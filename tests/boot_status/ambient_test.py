"""Check live LED settings with no emulator frames, using the real LED driver."""
from pathlib import Path
import ast,subprocess,tempfile
root=Path(__file__).resolve().parents[2]
stub='\n#include <stdbool.h>\n#include <stdint.h>\n#include <assert.h>\ntypedef unsigned uint;\ntypedef int PIO;\ntypedef int pio_sm_config;\n#define pio0 0\n#define PICO_OK 0\n#define PIO_FIFO_JOIN_TX 0\n#define clk_sys 0\nstatic int ws2812_program;\nstatic int base_result,claim_result,can_add=1,claimed,released,removed,writes,enabled,delayed;\nstatic uint32_t words[7];\nstatic int pio_set_gpio_base(PIO p,int b){assert(p==0&&b==16);return base_result;}\nstatic int pio_claim_unused_sm(PIO p,bool required){assert(!required);claimed++;return claim_result;}\nstatic bool pio_can_add_program(PIO p,void *x){return can_add;}\nstatic void pio_sm_unclaim(PIO p,int s){released++;}\nstatic uint pio_add_program(PIO p,void *x){return 0;}\nstatic void pio_gpio_init(PIO p,int pin){assert(pin==33);}\nstatic void pio_sm_set_consecutive_pindirs(PIO p,int s,int pin,int n,bool out){assert(pin==33&&n==1&&out);}\nstatic pio_sm_config ws2812_program_get_default_config(uint n){return 0;}\nstatic void sm_config_set_sideset_pins(pio_sm_config *c,int pin){assert(pin==33);}\nstatic void sm_config_set_out_shift(pio_sm_config *c,bool right,bool pull,int bits){assert(!right&&pull&&bits==24);}\nstatic void sm_config_set_fifo_join(pio_sm_config *c,int j){}\nstatic void sm_config_set_clkdiv(pio_sm_config *c,float f){assert(f==31.25f);}\nstatic unsigned clock_get_hz(int clk){return 250000000;}\nstatic void pio_sm_init(PIO p,int s,int offset,pio_sm_config *c){}\nstatic void pio_sm_set_enabled(PIO p,int s,bool on){enabled+=on?1:-1;}\nstatic void pio_sm_put_blocking(PIO p,int s,uint32_t w){assert(writes<7);words[writes++]=w;}\nstatic void busy_wait_us_32(unsigned us){assert(us==600);delayed++;}\nstatic void pio_remove_program(PIO p,void *x,int offset){removed++;}\n'
stub=stub.replace('assert(!required);','assert(required);')
source=(root/'src/boards/presto/presto_ambient.c').read_text()
source='\n'.join(s for s in source.splitlines() if not s.startswith('#include'))
extra=r'''
#include <stdio.h>
#include <string.h>
#include <math.h>
#define __not_in_flash_func(x) x
#define BOARD_DISPLAY_WIDTH 480
#define BOARD_DISPLAY_HEIGHT 480
#define ws2812_T1 2
#define ws2812_T2 5
#define ws2812_T3 3
static bool fifo_empty=true;
static unsigned wakes,reads;
static bool pio_sm_is_tx_fifo_empty(PIO p,uint sm){return fifo_empty;}
static void pio_sm_put(PIO p,uint sm,uint32_t value){assert(writes<7);words[writes++]=value;}
static void __sev(void){wakes++;}
static uint32_t time_us_32(void){return 100;}
static const uint16_t *board_display_row_rgb565(int y,uint16_t *buffer){reads++;return NULL;}
static const uint16_t *board_display_row(int y){reads++;return NULL;}
'''
main=r'''
int main(void){
 board_leds_init();writes=0;
 for(unsigned i=0;i<7;i++)for(unsigned ch=0;ch<3;ch++)s_level[i][ch]=255<<8;
 assert(!board_leds_due());board_leds_set(1,50,0);assert(board_leds_due()&&wakes);
 board_leds_update();assert(writes==7&&reads==0&&words[0]==0x7f7f7f00&&!board_leds_due());
 writes=0;board_leds_set(-1,100,-1);board_leds_update();assert(words[0]==0xffffff00&&reads==0);
 writes=0;board_leds_set(0,-1,-1);board_leds_update();for(unsigned i=0;i<7;i++)assert(words[i]==0);
 writes=0;board_leds_set(1,25,-1);fifo_empty=false;board_leds_update();assert(writes==0&&board_leds_due());
 fifo_empty=true;board_leds_update();assert(writes==7&&words[0]==0x3f3f3f00&&!board_leds_due());
 assert(s_frames==0&&reads==0);
 // Normal frame-driven ambient sampling resumes as before.
 writes=0;board_leds_frame(0);board_leds_frame(0);assert(board_leds_due());board_leds_update();assert(reads>0);
}
'''
with tempfile.TemporaryDirectory() as directory:
 p=Path(directory)
 for machine in ['MASTER','BEEB']:
  (p/'test.c').write_text(stub+extra+source+main)
  subprocess.run(['cc','-std=c11','-fsanitize=address,undefined',f'-DMACHINE_{machine}=1',str(p/'test.c'),'-o',str(p/'test')],check=True)
  subprocess.run([str(p/'test')],check=True)
print('Both BBC LED formats: live brightness/off/on without frames; FIFO retry; normal sampling resumes: PASS')
