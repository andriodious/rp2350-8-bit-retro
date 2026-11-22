"""Check live LED settings with no emulator frames, using the real LED driver."""
from pathlib import Path
import ast,subprocess,tempfile
root=Path(__file__).resolve().parents[2]
tree=ast.parse((root/'tests/startup/led_test.py').read_text())
stub=next(ast.literal_eval(n.value) for n in tree.body if isinstance(n,ast.Assign) and any(isinstance(t,ast.Name) and t.id=='stub' for t in n.targets))
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
 for machine in ['MASTER','BEEB','C64','SPECTRUM']:
  (p/'test.c').write_text(stub+extra+source+main)
  subprocess.run(['cc','-std=c11','-fsanitize=address,undefined',f'-DMACHINE_{machine}=1',str(p/'test.c'),'-o',str(p/'test')],check=True)
  subprocess.run([str(p/'test')],check=True)
print('All four LED formats: live brightness/off/on without frames; FIFO retry; normal sampling resumes: PASS')
