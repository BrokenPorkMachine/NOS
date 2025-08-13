#pragma once
#include <stdint.h>

/* Unified xAPIC/x2APIC interface */
void lapic_enable(void);
void lapic_eoi(void);
void lapic_timer_init(uint8_t vector);

/* Optional helpers used by SMP bootstrap */
uint32_t lapic_get_id(void);
void lapic_send_ipi(uint8_t apic_id, uint8_t vector);
void lapic_send_init(uint8_t apic_id);
void lapic_send_startup(uint8_t apic_id, uint8_t vector);
