#include <stdint.h>
#include "arch/APIC/lapic.h"

/* Adjust base if you remap LAPIC */
#define LAPIC_BASE  0xFEE00000ULL
#define LAPIC_EOI   0xB0

static inline void mmio_write32(uintptr_t addr, uint32_t val) {
    *(volatile uint32_t*)addr = val;
}

void lapic_eoi(void) {
    mmio_write32((uintptr_t)(LAPIC_BASE + LAPIC_EOI), 0);
}
