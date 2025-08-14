#pragma once
#include <stdint.h>

struct isr_context;

void i2c_init(void);
int i2c_read(uint8_t *out);
void i2c_register_callback(void (*cb)(uint8_t data));
void isr_i2c_handler(struct isr_context *ctx);
