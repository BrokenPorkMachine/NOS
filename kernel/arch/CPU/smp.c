#include "smp.h"
#include "lapic.h"
#include "../../../nosm/drivers/IO/serial.h"
#include <string.h>
#include <stdint.h>
#include <stddef.h>

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

static uint8_t  apic_map[APIC_MAX_IDS];      /* APIC ID -> cpu index */
static uint32_t index_map[APIC_MAX_IDS];     /* cpu index -> APIC ID */
static uint8_t  cpu_online[APIC_MAX_IDS];    /* cpu index -> online flag */
static uint32_t cpu_total      = 1;          /* logical CPUs detected (bounded by bootinfo) */
static uint32_t online_count   = 0;          /* CPUs which have reported online */

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

uint32_t smp_apic_to_index(uint32_t apic_id) {
    if (apic_id >= APIC_MAX_IDS) return 0xFFFFFFFFu;
    uint8_t idx = apic_map[apic_id];
    return (idx == INVALID_INDEX) ? 0xFFFFFFFFu : (uint32_t)idx;
}

uint32_t smp_index_to_apic(uint32_t cpu_index) {
    if (cpu_index >= cpu_total) return 0xFFFFFFFFu;
    return index_map[cpu_index];
}

void smp_mark_online(uint32_t apic_id) {
    uint32_t idx = smp_apic_to_index(apic_id);
    if (idx == 0xFFFFFFFFu) return;
    if (!__atomic_load_n(&cpu_online[idx], __ATOMIC_ACQUIRE)) {
        __atomic_store_n(&cpu_online[idx], 1, __ATOMIC_RELEASE);
        __atomic_fetch_add(&online_count, 1, __ATOMIC_RELAXED);
    }
}

int smp_all_online(void) {
    return __atomic_load_n(&online_count, __ATOMIC_ACQUIRE) >= cpu_total;
}

void smp_bootstrap(const bootinfo_t *bi) {
    if (!bi) return;

    /* Initialize map to invalid */
    memset(apic_map, INVALID_INDEX, sizeof(apic_map));
    for (size_t i = 0; i < APIC_MAX_IDS; ++i) {
        index_map[i] = 0xFFFFFFFFu;
        cpu_online[i] = 0;
    }

    /* Build APIC -> index map and clamp total */
    uint32_t n = (bi->cpu_count && bi->cpu_count <= APIC_MAX_IDS) ? bi->cpu_count : 1u;
    for (uint32_t i = 0; i < n; ++i) {
        uint32_t apic = bi->cpus[i].apic_id;
        if (apic < APIC_MAX_IDS) {
            apic_map[apic] = (uint8_t)i;
            index_map[i] = apic;
        }
    }
    cpu_total = n;
    online_count = 0;

    /* BSP is whoever we’re currently running on */
    uint32_t bsp_apic = lapic_get_id();
    uint32_t bsp_idx = smp_apic_to_index(bsp_apic);
    if (bsp_idx != 0xFFFFFFFFu) {
        cpu_online[bsp_idx] = 1;
        online_count = 1;
    }

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

        /* Wait up to ~100ms for AP to report online */
        for (unsigned t = 0; t < 100; ++t) {
            if (__atomic_load_n(&cpu_online[i], __ATOMIC_ACQUIRE)) break;
            udelay(1000); /* 1ms */
        }
        if (!__atomic_load_n(&cpu_online[i], __ATOMIC_ACQUIRE))
            serial_printf("[smp] AP idx=%u apic=%u failed to start\n", i, apic);
    }
}
