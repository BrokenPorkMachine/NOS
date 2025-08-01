#pragma once
#include <stdint.h>

void pic_remap(void);
void pic_set_mask(uint8_t irq, int enable);

