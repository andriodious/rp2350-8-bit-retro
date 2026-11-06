// Opt-in boot breadcrumbs. No flash writes, watchdog changes or retained RAM.
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "ws2812.pio.h"
#include "startup_diag.h"
bool __attribute__((weak)) startup_diag_ambient(unsigned stage) {(void)stage;return false;}
void startup_diag_mark(unsigned stage) {
    if (startup_diag_ambient(stage)) return;
    // Before ambient/display/Wi-Fi claim PIO0, borrow and then release it.
    PIO pio = pio0;
    if (pio_set_gpio_base(pio,16)!=PICO_OK) return;
    int sm=pio_claim_unused_sm(pio,false);
    if(sm<0)return;
    if(!pio_can_add_program(pio,&ws2812_program)){pio_sm_unclaim(pio,sm);return;}
    uint offset=pio_add_program(pio,&ws2812_program);
    pio_gpio_init(pio,33);pio_sm_set_consecutive_pindirs(pio,sm,33,1,true);
    pio_sm_config c=ws2812_program_get_default_config(offset);
    sm_config_set_sideset_pins(&c,33);sm_config_set_out_shift(&c,false,true,24);
    sm_config_set_fifo_join(&c,PIO_FIFO_JOIN_TX);
    sm_config_set_clkdiv(&c,(float)clock_get_hz(clk_sys)/8000000.0f);
    pio_sm_init(pio,sm,offset,&c);pio_sm_set_enabled(pio,sm,true);
    for(unsigned i=0;i<7;i++)pio_sm_put_blocking(pio,sm,startup_diag_pixel(stage,i));
    // Seven pixels take 210 us; allow output and reset/latch time before release.
    busy_wait_us_32(600);
    pio_sm_set_enabled(pio,sm,false);pio_remove_program(pio,&ws2812_program,offset);pio_sm_unclaim(pio,sm);
}
