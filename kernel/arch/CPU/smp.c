#include "smp.h"
#include "lapic.h"
#include "../../drivers/IO/serial.h"
#include <string.h>
#include <stdint.h>

#define APIC_MAX_IDS   256
#define INVALID_INDEX  0xFF
#define SIPI_VECTOR    0x10   /* 0x10 -> 0x10000 (startup page), must match AP trampoline */

/* Simple udelay: crude spin; replace with TSC/PIT when available */
static void udelay(unsigned us) {
    /* ~ calibrated no-op; the exact time isn’t critical for SIPI spacing */
    for (volatile unsigned i = 0; i < us * 200; ++i) {
        __asm__ volatile("pause");
    }
}

static uint8_t  apic_map[APIC_MAX_IDS]; /* APIC ID -> cpu index */
static uint32_t cpu_total = 1;          /* logical CPUs detected (bounded by bootinfo) */

uint32_t smp_cpu_id(void) {
    return lapic_get_id();
}

uint32_t smp_cpu_index(void) {
    uint32_t apic = smp_cpu_id();
    uint8_t idx = apic < APIC_MAX_IDS ? apic_map[apic] : INVALID_INDEX;
    return (idx == INVALID_INDEX) ? 0u : (uint32_t)idx;
}

uint32_t smp_cpu_count(void) {
    return cpu_total;
}

/* optional: call from AP entry once it’s alive, if you keep an online counter
   void smp_mark_online(uint32_t apic_id) { ... } */

void smp_bootstrap(const bootinfo_t *bi) {
    if (!bi) return;

    /* Initialize map to invalid */
    memset(apic_map, INVALID_INDEX, sizeof(apic_map));

    /* Build APIC -> index map and clamp total */
    uint32_t n = (bi->cpu_count && bi->cpu_count <= APIC_MAX_IDS) ? bi->cpu_count : 1u;
    for (uint32_t i = 0; i < n; ++i) {
        uint32_t apic = bi->cpus[i].apic_id;
        if (apic < APIC_MAX_IDS) apic_map[apic] = (uint8_t)i;
    }
    cpu_total = n;

    /* BSP is whoever we’re currently running on */
    uint32_t bsp_apic = lapic_get_id();

    serial_printf("[smp] BSP APIC=%u, CPUs (bootinfo)=%u\n", bsp_apic, cpu_total);

    /* Bring up APs per Intel SDM: INIT (level assert, level trigger), then two SIPIs */
    for (uint32_t i = 0; i < n; ++i) {
        uint32_t apic = bi->cpus[i].apic_id;
        if (apic == bsp_apic) continue;

        serial_printf("[smp] start AP idx=%u apic=%u\n", i, apic);

        /* INIT IPI */
        lapic_send_init((uint8_t)apic);
        udelay(10000); /* ~10 ms is conservative; many OSes use 10ms */

        /* SIPI #1 */
        lapic_send_startup((uint8_t)apic, SIPI_VECTOR);
        udelay(200);   /* 200 µs spacing (Intel rec: 200µs+) */

        /* SIPI #2 (recommended even if #1 succeeded) */
        lapic_send_startup((uint8_t)apic, SIPI_VECTOR);
        udelay(200);

        /* Optionally: wait for AP to signal online here (IPI ping or shared flag) */
        /* TODO: hook a per-CPU online flag and timeout if desired. */
    }
}
