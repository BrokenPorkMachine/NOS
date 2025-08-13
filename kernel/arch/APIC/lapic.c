#include <stdint.h>
#include "arch/APIC/lapic.h"

#define LAPIC_BASE   0xFEE00000ULL
#define LAPIC_EOI    0xB0
#define LAPIC_SVR    0xF0
#define LAPIC_LVT_TMR 0x320
#define LAPIC_TMR_INITCNT 0x380
#define LAPIC_TMR_CURRCNT 0x390
#define LAPIC_TMR_DIV   0x3E0

static inline void mmio_write32(uintptr_t off, uint32_t v) {
    *(volatile uint32_t*)(off) = v;
}
static inline uint32_t mmio_read32(uintptr_t off) {
    return *(volatile uint32_t*)(off);
}

void lapic_eoi(void) {
    mmio_write32((uintptr_t)(LAPIC_BASE + LAPIC_EOI), 0);
}

void lapic_enable(void) {
    /* Set Spurious Interrupt Vector Register: enable bit (8) + vector (we’ll keep 0xFF) */
    uint32_t svr = 0x100 | 0xFF;
    mmio_write32((uintptr_t)(LAPIC_BASE + LAPIC_SVR), svr);
}

void lapic_timer_init(uint8_t vector) {
    /* Divide by 16 */
    mmio_write32((uintptr_t)(LAPIC_BASE + LAPIC_TMR_DIV), 0x3 /* divide by 16 */);

    /* One-shot timer: write LVT with vector, unmasked */
    uint32_t lvt = vector; /* one-shot = mode 0 */
    mmio_write32((uintptr_t)(LAPIC_BASE + LAPIC_LVT_TMR), lvt & 0xFF);

    /* Load initial count (pick a test value; tune later) */
    mmio_write32((uintptr_t)(LAPIC_BASE + LAPIC_TMR_INITCNT), 10'000'000u);
}
