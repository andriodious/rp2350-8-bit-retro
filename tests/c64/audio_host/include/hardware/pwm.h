#pragma once
#include "pico/stdlib.h"
typedef struct { unsigned wrap; } pwm_config;
typedef struct { volatile uint32_t cc; } pwm_slice_hw_t;
typedef struct { pwm_slice_hw_t slice[8]; } pwm_hw_t;
extern pwm_hw_t *pwm_hw;
unsigned pwm_gpio_to_slice_num(unsigned pin);
pwm_config pwm_get_default_config(void);
void pwm_config_set_wrap(pwm_config *c, unsigned wrap);
void pwm_init(unsigned slice, const pwm_config *c, bool enabled);
void pwm_set_enabled(unsigned slice, bool enabled);
