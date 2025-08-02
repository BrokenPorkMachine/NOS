#pragma once
#include <stdint.h>

void lapic_init(uintptr_t base);
uint32_t lapic_get_id(void);
void lapic_eoi(void);
void lapic_send_ipi(uint8_t apic_id, uint8_t vector);
void lapic_send_init(uint8_t apic_id);
void lapic_send_startup(uint8_t apic_id, uint8_t vector);
