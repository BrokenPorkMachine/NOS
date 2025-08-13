#include "lapic.h"
#include <stdint.h>

/* ---------- Local APIC register offsets (xAPIC MMIO) ---------- */
enum {
    LAPIC_ID          = 0x020, /* Local APIC ID (RO) */
    LAPIC_VER         = 0x030, /* Version */
    LAPIC_TPR         = 0x080, /* Task Priority */
    LAPIC_EOI         = 0x0B0, /* End of Interrupt (WO) */
    LAPIC_SVR         = 0x0F0, /* Spurious Interrupt Vector Register */
    LAPIC_ICR_LOW     = 0x300, /* Interrupt Command Register (low dword) */
    LAPIC_ICR_HIGH    = 0x310, /* ICR high dword (dest APIC ID) */

    LAPIC_LVT_TIMER   = 0x320,
    LAPIC_LVT_LINT0   = 0x350,
    LAPIC_LVT_LINT1   = 0x360,
    LAPIC_LVT_ERROR   = 0x370,

    LAPIC_TICR        = 0x380, /* Timer Initial Count (RW) */
    LAPIC_TCCR        = 0x390, /* Timer Current Count (RO) */
    LAPIC_TDCR        = 0x3E0, /* Timer Divide Configuration */
};

/* ---------- Bitfields ---------- */
#define SVR_SW_ENABLE       (1u << 8)    /* Software enable APIC */
#define LVT_MASKED          (1u << 16)
#define LVT_TIMER_PERIODIC  (1u << 17)

#define ICR_DELIVERY_FIXED  (0u   << 8)
#define ICR_DELIVERY_INIT   (0x5u << 8)
#define ICR_DELIVERY_STARTUP (0x6u << 8)

#define ICR_LEVEL_ASSERT    (1u << 14)
#define ICR_TRIGGER_LEVEL   (1u << 15)
#define ICR_DESTMODE_PHYS   (0u << 11)

#define ICR_DELIV_STATUS    (1u << 12)   /* 1 = in progress */

/* TDCR divide values (encode) */
enum {
    TDCR_DIV_2   = 0x0,
    TDCR_DIV_4   = 0x1,
    TDCR_DIV_8   = 0x2,
    TDCR_DIV_16  = 0x3,
    TDCR_DIV_32  = 0x8,
    TDCR_DIV_64  = 0x9,
    TDCR_DIV_128 = 0xA,
    TDCR_DIV_1   = 0xB,
};

/* ---------- MMIO helpers ---------- */

static volatile uint32_t *lapic = 0;

static inline void lapic_write(uint32_t reg, uint32_t val) {
    /* MMIO is strongly ordered on x86, but post a read to defeat write posting. */
    lapic[reg / 4] = val;
    (void)lapic[LAPIC_ID / 4];
}
static inline uint32_t lapic_read(uint32_t reg) {
    return lapic[reg / 4];
}

static inline void wait_icr_idle(void) {
    /* Wait until any previous IPI has completed (Delivery Status = 0). */
    while (lapic_read(LAPIC_ICR_LOW) & ICR_DELIV_STATUS) { }
}

/* ---------- Public API (kept compatible with your originals) ---------- */

void lapic_init(uintptr_t base) {
    lapic = (volatile uint32_t *)base;
    if (!lapic) return;

    /* Enable APIC with a sane spurious vector (0xFF by default). */
    uint32_t svr = lapic_read(LAPIC_SVR);
    uint8_t vector = (uint8_t)(svr & 0xFF);
    if (vector == 0) vector = 0xFF; /* avoid vector 0 */
    svr = (svr & ~0xFFu) | vector | SVR_SW_ENABLE;
    lapic_write(LAPIC_SVR, svr);

    /* Unmask error LVT and mask LINT lines by default (often wired to IOAPIC). */
    uint32_t lvt_err = lapic_read(LAPIC_LVT_ERROR) & ~LVT_MASKED;
    lapic_write(LAPIC_LVT_ERROR, lvt_err);

    lapic_write(LAPIC_LVT_LINT0, lapic_read(LAPIC_LVT_LINT0) | LVT_MASKED);
    lapic_write(LAPIC_LVT_LINT1, lapic_read(LAPIC_LVT_LINT1) | LVT_MASKED);

    /* Default: accept all priorities (TPR=0) */
    lapic_write(LAPIC_TPR, 0);
}

uint32_t lapic_get_id(void) {
    if (!lapic) return 0;
    return lapic_read(LAPIC_ID) >> 24;
}

/* Fire a simple fixed IPI with given vector to a specific APIC ID (physical dest). */
void lapic_send_ipi(uint8_t apic_id, uint8_t vector) {
    if (!lapic) return;
    wait_icr_idle();
    lapic_write(LAPIC_ICR_HIGH, ((uint32_t)apic_id) << 24);
    lapic_write(LAPIC_ICR_LOW,  (uint32_t)vector | ICR_DELIVERY_FIXED | ICR_DESTMODE_PHYS);
}

void lapic_send_init(uint8_t apic_id) {
    if (!lapic) return;
    wait_icr_idle();
    lapic_write(LAPIC_ICR_HIGH, ((uint32_t)apic_id) << 24);
    lapic_write(LAPIC_ICR_LOW,  ICR_DELIVERY_INIT | ICR_LEVEL_ASSERT | ICR_TRIGGER_LEVEL);
}

void lapic_send_startup(uint8_t apic_id, uint8_t vector) {
    if (!lapic) return;
    wait_icr_idle();
    lapic_write(LAPIC_ICR_HIGH, ((uint32_t)apic_id) << 24);
    lapic_write(LAPIC_ICR_LOW,  ICR_DELIVERY_STARTUP | (vector & 0xFF));
}

/* ---------- Optional helpers (use if you want a basic APIC timer) ---------- */

void lapic_timer_setup_periodic(uint8_t vector, uint32_t initial_count, uint8_t tdcr_div_code) {
    if (!lapic) return;
    lapic_write(LAPIC_TDCR, tdcr_div_code);
    lapic_write(LAPIC_LVT_TIMER, (uint32_t)vector | LVT_TIMER_PERIODIC);
    lapic_write(LAPIC_TICR, initial_count);
}

void lapic_timer_setup_oneshot(uint8_t vector, uint32_t initial_count, uint8_t tdcr_div_code) {
    if (!lapic) return;
    lapic_write(LAPIC_TDCR, tdcr_div_code);
    lapic_write(LAPIC_LVT_TIMER, (uint32_t)vector); /* one-shot: clear periodic bit */
    lapic_write(LAPIC_TICR, initial_count);
}

uint32_t lapic_timer_current(void) {
    if (!lapic) return 0;
    return lapic_read(LAPIC_TCCR);
}
