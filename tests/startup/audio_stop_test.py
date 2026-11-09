"""Check the actual core-1 shutdown sequence with hardware assertions."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[2]
s=(root/'src/machines/c64/audio.c').read_text()
body=s.split('void c64_audio_diag_poll(void) {',1)[1].split('\n#endif',1)[0]
code=r'''
#include <cassert>
#include <cstdint>
static bool initialized,stop_requested,dma_stopped;
static int channels[2]={2,3},audio_timer=1,aborts=0,acks=0;
static bool irq=true,channel_irq[4]={true,true,true,true},timer=true,pwm=true,low=false,output=false;
#define DMA_IRQ_1 1
#define DMA_CH0_CTRL_TRIG_EN_BITS 1
#define BOARD_AUDIO_PWM_PIN 43
#define GPIO_OUT 1
struct Channel{uint32_t ctrl_trig;};struct DMA{Channel ch[4];} hardware;
static DMA *dma_hw=&hardware;
void irq_set_enabled(int n,bool e){assert(n==1&&!e);irq=e;}
void dma_irqn_set_channel_enabled(int n,int c,bool e){assert(n==1&&!irq&&!e);channel_irq[c]=e;}
void dma_timer_set_fraction(int t,int n,int d){assert(t==1&&n==0&&d==1);timer=false;}
void hw_clear_bits(uint32_t *p,uint32_t b){assert(!timer);*p&=~b;}
void dma_channel_abort(int c){assert(c==2||c==3);assert(!irq&&!channel_irq[2]&&!channel_irq[3]);assert(!hardware.ch[2].ctrl_trig&&!hardware.ch[3].ctrl_trig);aborts++;}
void dma_irqn_acknowledge_channel(int n,int c){assert(n==1&&(c==2||c==3));acks++;}
int pwm_gpio_to_slice_num(int p){assert(p==43);return 5;}
void pwm_set_enabled(int s,bool e){assert(s==5&&!e&&aborts==2);pwm=e;}
void gpio_init(int p){assert(p==43&&!pwm);}
void gpio_put(int p,int v){assert(p==43&&v==0);low=true;}
void gpio_set_dir(int p,int v){assert(p==43&&v==1&&low);output=true;}
'''+ 'void c64_audio_diag_poll(void) {'+body+r'''
int main(){
 hardware.ch[2].ctrl_trig=hardware.ch[3].ctrl_trig=1;
 c64_audio_diag_poll();assert(aborts==0);
 initialized=true;c64_audio_diag_poll();assert(aborts==0);
 stop_requested=true;c64_audio_diag_poll();assert(dma_stopped&&aborts==2&&acks==2&&output);
 c64_audio_diag_poll();assert(aborts==2);
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'test.cpp';p.write_text(code);exe=Path(d)/'test'
 subprocess.run(['c++','-std=c++17','-fsanitize=address,undefined',str(p),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
print('Audio shutdown: guarded, both chains disabled before abort, IRQ/timer/PWM stopped, idempotent')
