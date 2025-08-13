#pragma once
#include <stdint.h>

void lapic_eoi(void);
void lapic_enable(void);
void lapic_timer_init(uint8_t vector);
