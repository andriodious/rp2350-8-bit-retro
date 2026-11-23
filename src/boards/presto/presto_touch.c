// Presto FT6236: Pimoroni touch.py pin/register layout. Main-core polling only.
#include "board.h"
#include "pico/stdlib.h"
#include "hardware/i2c.h"
static void touch_bus_init(void) {
    // Release a touch controller left mid-transfer by a physical reset.
    gpio_init(BOARD_TOUCH_SDA_PIN);gpio_set_dir(BOARD_TOUCH_SDA_PIN,GPIO_IN);gpio_pull_up(BOARD_TOUCH_SDA_PIN);
    gpio_init(BOARD_TOUCH_SCL_PIN);gpio_put(BOARD_TOUCH_SCL_PIN,0);gpio_set_dir(BOARD_TOUCH_SCL_PIN,GPIO_IN);gpio_pull_up(BOARD_TOUCH_SCL_PIN);
    for(unsigned i=0;i<9&&!gpio_get(BOARD_TOUCH_SDA_PIN);i++) {
        gpio_set_dir(BOARD_TOUCH_SCL_PIN,GPIO_OUT);sleep_us(5);gpio_set_dir(BOARD_TOUCH_SCL_PIN,GPIO_IN);sleep_us(5);
    }
    i2c_init(BOARD_TOUCH_I2C,400000);gpio_set_function(BOARD_TOUCH_SDA_PIN,GPIO_FUNC_I2C);gpio_set_function(BOARD_TOUCH_SCL_PIN,GPIO_FUNC_I2C);
}
bool board_touch(unsigned *x,unsigned *y) {
    static bool ready,held;static uint64_t retry;static unsigned last_x,last_y;
    *x=last_x;*y=last_y;
    if(!ready){touch_bus_init();gpio_init(BOARD_TOUCH_INT_PIN);gpio_set_dir(BOARD_TOUCH_INT_PIN,GPIO_IN);gpio_pull_up(BOARD_TOUCH_INT_PIN);ready=true;}
    if(time_us_64()<retry)return held;
    if(gpio_get(BOARD_TOUCH_INT_PIN)&&!held)return false;
    uint8_t reg=0,data[9];
    if(i2c_write_timeout_us(BOARD_TOUCH_I2C,BOARD_TOUCH_ADDR,&reg,1,true,1000)!=1 || i2c_read_timeout_us(BOARD_TOUCH_I2C,BOARD_TOUCH_ADDR,data,9,false,1000)!=9){retry=time_us_64()+500000;return held;}
    unsigned event=data[3]>>6;
    held=(data[2]&15)==1 && (event==0||event==2);
    *x=((data[3]&15)<<8)|data[4];*y=((data[5]&15)<<8)|data[6];
    held=held && *x<480 && *y<480;
    if(held){last_x=*x;last_y=*y;}
    return held;
}
