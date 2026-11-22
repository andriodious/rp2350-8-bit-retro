#pragma once
#include "pico/stdlib.h"
#define DMA_SIZE_32 2
#define DMA_CH0_CTRL_TRIG_EN_BITS 1
typedef struct { unsigned size, ring_bits, dreq, chain; bool read_inc, write_inc, ring_write; } dma_channel_config;
typedef struct { uint32_t ctrl_trig; } dma_channel_hw_t;
typedef struct { dma_channel_hw_t ch[4]; } dma_hw_t;
extern dma_hw_t *dma_hw;
int dma_claim_unused_timer(bool required);
void dma_timer_set_fraction(unsigned timer, unsigned numerator, unsigned denominator);
int dma_claim_unused_channel(bool required);
dma_channel_config dma_channel_get_default_config(unsigned channel);
void channel_config_set_transfer_data_size(dma_channel_config *c, unsigned value);
void channel_config_set_read_increment(dma_channel_config *c, bool value);
void channel_config_set_ring(dma_channel_config *c, bool write, unsigned bits);
void channel_config_set_write_increment(dma_channel_config *c, bool value);
void channel_config_set_dreq(dma_channel_config *c, unsigned value);
void channel_config_set_chain_to(dma_channel_config *c, unsigned value);
unsigned dma_get_timer_dreq(unsigned timer);
void dma_channel_configure(unsigned channel, const dma_channel_config *c, volatile void *write, const void *read, unsigned count, bool trigger);
void dma_irqn_set_channel_enabled(unsigned irq, unsigned channel, bool enabled);
void dma_channel_start(unsigned channel);
bool dma_irqn_get_channel_status(unsigned irq, unsigned channel);
void dma_irqn_acknowledge_channel(unsigned irq, unsigned channel);
void dma_channel_set_read_addr(unsigned channel, const void *address, bool trigger);
void dma_channel_abort(unsigned channel);
bool dma_channel_is_busy(unsigned channel);
void hw_clear_bits(uint32_t *p, uint32_t mask);
