// kernel/arch/idt_guard.c
// Minimal, SSE-free IDT "guard": prints pre/post state; exports idt_guard_init_once()
// so other code can call it without link errors. Doesn't install __attribute__((interrupt))
// handlers (avoids GCC "SSE not allowed" errors). Safe to call multiple times.

#include <stdint.h>
#include <stddef.h>
#include "../../include/printf.h"
#include "../../nosm/drivers/IO/serial.h"

#define VEC_UD  6
#define VEC_GP  13

#pragma pack(push,1)
typedef struct {
    uint16_t limit;
    uint64_t base;
} idtr_t;

typedef struct {
    uint16_t off_lo;
    uint16_t sel;
    uint8_t  ist;
    uint8_t  type_attr;
    uint16_t off_mid;
    uint32_t off_hi;
    uint32_t zero;
} idt_gate_t;
#pragma pack(pop)

static inline void sidt(idtr_t *d) {
    __asm__ volatile ("sidt %0" : "=m"(*d));
}

static inline uint64_t gate_target(const idt_gate_t *g) {
    return ((uint64_t)g->off_lo) | (((uint64_t)g->off_mid) << 16) | (((uint64_t)g->off_hi) << 32);
}

// Exported (non-static) symbol so other translation units can call it.
void idt_guard_init_once(void) {
    static int done = 0;
    if (done) return;

    idtr_t d = {0};
    sidt(&d);

    // IDT sanity
    if (d.base == 0 || d.limit < (sizeof(idt_gate_t) * (VEC_GP + 1) - 1)) {
        serial_printf("[idt] invalid IDT: base=%016lx limit=%04x\n", (unsigned long)d.base, d.limit);
        done = 1;
        return;
    }

    idt_gate_t *idt = (idt_gate_t *)(uintptr_t)d.base;
    uint64_t pre6  = gate_target(&idt[VEC_UD]);
    uint64_t pre13 = gate_target(&idt[VEC_GP]);

    serial_printf("[idt] pre-guard base=%016lx lim=%04x vec6=%016lx vec13=%016lx\n",
                  (unsigned long)d.base, d.limit, (unsigned long)pre6, (unsigned long)pre13);

    // NOTE: We don't patch the IDT here to avoid ISR attribute/SSE hazards.
    // If you later want to redirect UD/GP to a mux, do it with tiny hand-written
    // asm stubs (no SSE) and set off_* fields explicitly. For now just log.

    uint64_t post6  = gate_target(&idt[VEC_UD]);
    uint64_t post13 = gate_target(&idt[VEC_GP]);

    serial_printf("[idt] post-guard base=%016lx lim=%04x vec6=%016lx vec13=%016lx\n",
                  (unsigned long)d.base, d.limit, (unsigned long)post6, (unsigned long)post13);

    done = 1;
}
