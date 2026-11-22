#pragma once
#include "pico/stdlib.h"
#define DMA_IRQ_1 1
void irq_set_exclusive_handler(unsigned irq, void (*handler)(void));
void irq_set_enabled(unsigned irq, bool enabled);
