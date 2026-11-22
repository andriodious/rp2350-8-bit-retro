#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define __not_in_flash_func(name) name
#define __dmb() ((void)0)
#define __sev() ((void)0)
#define GPIO_FUNC_PWM 4
#define GPIO_OUT 1
void gpio_set_function(unsigned pin, unsigned function);
void gpio_init(unsigned pin);
void gpio_put(unsigned pin, unsigned value);
void gpio_set_dir(unsigned pin, unsigned value);
