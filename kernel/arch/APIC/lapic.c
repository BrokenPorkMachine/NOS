#include "lapic.h"
#include <stdint.h>

#define MSR_IA32_APIC_BASE 0x1B

static volatile uint32_t *lapic_mmio = (volatile uint32_t *)0xFEE00000;
static int lapic_x2apic = 0;

static inline uint64_t rdmsr(uint32_t msr) {
    uint32_t lo, hi;
    __asm__ volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return ((uint64_t)hi << 32) | lo;
}

static inline void wrmsr(uint32_t msr, uint64_t val) {
    __asm__ volatile("wrmsr" :: "c"(msr), "a"((uint32_t)val), "d"((uint32_t)(val >> 32)));
}

static inline void lapic_write(uint32_t reg, uint32_t val) {
    lapic_mmio[reg / 4] = val;
    (void)lapic_mmio[0x20 / 4];
}

static inline uint32_t lapic_read(uint32_t reg) {
    return lapic_mmio[reg / 4];
}

void lapic_eoi(void) {
    if (lapic_x2apic)
        wrmsr(0x80B, 0);
    else
        lapic_write(0x0B0, 0);
}

void lapic_enable(void) {
    uint32_t a, b, c, d;
    __asm__ volatile("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(1));
    lapic_x2apic = (c >> 21) & 1;

    uint64_t base = rdmsr(MSR_IA32_APIC_BASE);
    lapic_mmio = (volatile uint32_t *)(uintptr_t)(base & 0xFFFFF000u);
    base |= (1ULL << 11); /* enable */
    if (lapic_x2apic) {
        base |= (1ULL << 10); /* x2APIC */
        wrmsr(MSR_IA32_APIC_BASE, base);
        wrmsr(0x80F, 0x100 | 0xFF); /* SVR enable + vector */
        wrmsr(0x808, 0);            /* TPR=0 */
    } else {
        wrmsr(MSR_IA32_APIC_BASE, base);
        lapic_write(0x0F0, 0x100 | 0xFF);
        lapic_write(0x080, 0);
    }
}

void lapic_timer_init(uint8_t vector) {
    if (lapic_x2apic) {
        wrmsr(0x83E, 0x3); /* divide by 16 */
        wrmsr(0x832, (uint32_t)vector | (1u << 17));
        wrmsr(0x838, 10000000u);
    } else {
        lapic_write(0x3E0, 0x3);
        lapic_write(0x320, (uint32_t)vector | (1u << 17));
        lapic_write(0x380, 10000000u);
    }
}

uint32_t lapic_get_id(void) {
    if (lapic_x2apic)
        return (uint32_t)rdmsr(0x802);
    return lapic_read(0x020) >> 24;
}

static inline void wait_icr_idle(void) {
    if (!lapic_x2apic) {
        while (lapic_read(0x300) & (1u << 12)) { }
    }
}

void lapic_send_ipi(uint8_t apic_id, uint8_t vector) {
    if (lapic_x2apic) {
        uint64_t icr = ((uint64_t)apic_id << 32) | vector;
        wrmsr(0x830, icr);
    } else {
        wait_icr_idle();
        lapic_write(0x310, ((uint32_t)apic_id) << 24);
        lapic_write(0x300, (uint32_t)vector);
    }
}

void lapic_send_init(uint8_t apic_id) {
    if (lapic_x2apic) {
        uint64_t icr = ((uint64_t)apic_id << 32) | (5ULL << 8) | (1ULL << 14) | (1ULL << 15);
        wrmsr(0x830, icr);
    } else {
        wait_icr_idle();
        lapic_write(0x310, ((uint32_t)apic_id) << 24);
        lapic_write(0x300, (0x5u << 8) | (1u << 14) | (1u << 15));
    }
}

void lapic_send_startup(uint8_t apic_id, uint8_t vector) {
    if (lapic_x2apic) {
        uint64_t icr = ((uint64_t)apic_id << 32) | (6ULL << 8) | (vector & 0xFF);
        wrmsr(0x830, icr);
    } else {
        wait_icr_idle();
        lapic_write(0x310, ((uint32_t)apic_id) << 24);
        lapic_write(0x300, (0x6u << 8) | (vector & 0xFF));
    }
}
