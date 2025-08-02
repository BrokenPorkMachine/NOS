#include "lapic.h"

static volatile uint32_t *lapic = 0;

void lapic_init(uintptr_t base) {
    lapic = (volatile uint32_t *)base;
    if (!lapic)
        return;
    // Enable by setting bit 8 in spurious interrupt register
    lapic[0xF0/4] |= 0x100;
}

uint32_t lapic_get_id(void) {
    if (!lapic) return 0;
    return lapic[0x20/4] >> 24;
}

void lapic_eoi(void) {
    if (!lapic) return;
    lapic[0xB0/4] = 0;
}

void lapic_send_ipi(uint8_t apic_id, uint8_t vector) {
    if (!lapic) return;
    lapic[0x310/4] = ((uint32_t)apic_id) << 24;
    lapic[0x300/4] = vector;
}

void lapic_send_init(uint8_t apic_id) {
    if (!lapic) return;
    lapic[0x310/4] = ((uint32_t)apic_id) << 24;
    lapic[0x300/4] = 0x4500; // INIT IPI
}

void lapic_send_startup(uint8_t apic_id, uint8_t vector) {
    if (!lapic) return;
    lapic[0x310/4] = ((uint32_t)apic_id) << 24;
    lapic[0x300/4] = 0x4600 | vector; // SIPI
}
