#include <stdint.h>
#include "arch/APIC/lapic.h"

#define LAPIC_BASE        0xFEE00000ULL
#define LAPIC_REG(off)    ((uintptr_t)(LAPIC_BASE + (off)))

#define LAPIC_EOI         0xB0
#define LAPIC_SVR         0xF0
#define LAPIC_LVT_TMR     0x320
#define LAPIC_TMR_INITCNT 0x380
#define LAPIC_TMR_CURRCNT 0x390
#define LAPIC_TMR_DIV     0x3E0

static inline void mmio_write32(uintptr_t addr, uint32_t val) {
    *(volatile uint32_t*)addr = val;
}
static inline uint32_t mmio_read32(uintptr_t addr) {
    return *(volatile uint32_t*)addr;
}

void lapic_eoi(void) {
    mmio_write32(LAPIC_REG(LAPIC_EOI), 0);
}

void lapic_enable(void) {
    /* Enable bit (bit 8) + keep spurious vector at 0xFF */
    uint32_t svr = 0x100 | 0xFF;
    mmio_write32(LAPIC_REG(LAPIC_SVR), svr);
    (void)mmio_read32(LAPIC_REG(LAPIC_SVR)); /* post */
}

void lapic_timer_init(uint8_t vector) {
    /* Divide by 16 */
    mmio_write32(LAPIC_REG(LAPIC_TMR_DIV), 0x3);

    /* One-shot timer: write vector, unmasked */
    mmio_write32(LAPIC_REG(LAPIC_LVT_TMR), (uint32_t)vector & 0xFF);

    /* Kick it with a test count (tune later) */
    mmio_write32(LAPIC_REG(LAPIC_TMR_INITCNT), 10000000u);
}
