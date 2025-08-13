#include <stdint.h>
#include "lapic.h"

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
