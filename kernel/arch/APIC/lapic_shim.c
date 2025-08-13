#include <stdint.h>
#include "lapic.h"

extern void lapic_eoi(void);  /* defined in your existing LAPIC module */

#define LAPIC_BASE        0xFEE00000ULL
#define LAPIC_REG(off)    ((uintptr_t)(LAPIC_BASE + (off)))

#define LAPIC_SVR         0xF0
#define LAPIC_LVT_TMR     0x320
#define LAPIC_TMR_INITCNT 0x380
#define LAPIC_TMR_DIV     0x3E0

static inline void mmio_write32(uintptr_t addr, uint32_t val) {
    *(volatile uint32_t*)addr = val;
}
static inline uint32_t mmio_read32(uintptr_t addr) {
    return *(volatile uint32_t*)addr;
}

void lapic_enable(void) {
    /* Enable bit (8) + spurious vector (0xFF) */
    uint32_t svr = 0x100 | 0xFF;
    mmio_write32(LAPIC_REG(LAPIC_SVR), svr);
    (void)mmio_read32(LAPIC_REG(LAPIC_SVR));
}

void lapic_timer_init(uint8_t vector) {
    /* Divide by 16 */
    mmio_write32(LAPIC_REG(LAPIC_TMR_DIV), 0x3);
    /* One-shot, unmasked, vector in low 8 bits */
    mmio_write32(LAPIC_REG(LAPIC_LVT_TMR), (uint32_t)(vector & 0xFF));
    /* Kick a test count (tune later) */
    mmio_write32(LAPIC_REG(LAPIC_TMR_INITCNT), 10000000u);
}
