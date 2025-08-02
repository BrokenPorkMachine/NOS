#include "smp.h"
#include "lapic.h"
#include "../../drivers/IO/serial.h"

static uint8_t apic_map[256];
static uint32_t cpu_total = 1;

uint32_t smp_cpu_id(void) {
    return lapic_get_id();
}

uint32_t smp_cpu_index(void) {
    return apic_map[smp_cpu_id()];
}

uint32_t smp_cpu_count(void) {
    return cpu_total;
}

void smp_bootstrap(const bootinfo_t *bi) {
    if (!bi) return;
    cpu_total = bi->cpu_count ? bi->cpu_count : 1;
    for (uint32_t i = 0; i < bi->cpu_count && i < 256; ++i)
        apic_map[bi->cpus[i].apic_id] = i;

    uint32_t bsp = lapic_get_id();
    for (uint32_t i = 0; i < bi->cpu_count; ++i) {
        uint32_t apic = bi->cpus[i].apic_id;
        if (apic == bsp) continue;
        serial_puts("[smp] start AP\n");
        lapic_send_init(apic);
        for (volatile int j = 0; j < 100000; ++j) __asm__ volatile("pause");
        lapic_send_startup(apic, 0x10);
    }
}
