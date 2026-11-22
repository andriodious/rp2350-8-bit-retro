// Presto FT6236: Pimoroni touch.py pin/register layout. Main-core polling only.
#include "board.h"
#include "pico/stdlib.h"
#include "hardware/i2c.h"
static void touch_bus_init(void) {
    // Release a touch controller left mid-transfer by a physical reset.
    gpio_init(30);gpio_set_dir(30,GPIO_IN);gpio_pull_up(30);
    gpio_init(31);gpio_put(31,0);gpio_set_dir(31,GPIO_IN);gpio_pull_up(31);
    for(unsigned i=0;i<9&&!gpio_get(30);i++) {
        gpio_set_dir(31,GPIO_OUT);sleep_us(5);gpio_set_dir(31,GPIO_IN);sleep_us(5);
    }
    i2c_init(i2c1,400000);gpio_set_function(30,GPIO_FUNC_I2C);gpio_set_function(31,GPIO_FUNC_I2C);
}
bool board_touch(unsigned *x,unsigned *y) {
    static bool ready,held;static uint64_t retry;static unsigned last_x,last_y;
    *x=last_x;*y=last_y;
    if(!ready){touch_bus_init();gpio_init(32);gpio_set_dir(32,GPIO_IN);gpio_pull_up(32);ready=true;}
    if(time_us_64()<retry)return held;
    if(gpio_get(32)&&!held)return false;
    uint8_t reg=0,data[9];
    if(i2c_write_timeout_us(i2c1,0x48,&reg,1,true,1000)!=1 || i2c_read_timeout_us(i2c1,0x48,data,9,false,1000)!=9){retry=time_us_64()+500000;return held;}
    unsigned event=data[3]>>6;
    held=(data[2]&15)==1 && (event==0||event==2);
    *x=((data[3]&15)<<8)|data[4];*y=((data[5]&15)<<8)|data[6];
    held=held && *x<480 && *y<480;
    if(held){last_x=*x;last_y=*y;}
    return held;
}
