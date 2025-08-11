#include <stdint.h>
#include "../../nosm/drivers/IO/serial.h"
#include "idt_guard.h"

struct __attribute__((packed)) idtr_t {
    uint16_t limit;
    uint64_t base;
};

struct __attribute__((packed)) idt_gate_t {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t  ist;
    uint8_t  type_attr;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t zero;
};

static inline uint64_t gate_target(const struct idt_gate_t* g) {
    return (uint64_t)g->offset_low |
           ((uint64_t)g->offset_mid  << 16) |
           ((uint64_t)g->offset_high << 32);
}

void idt_guard_init_once(void) {
    static int done = 0;
    if (done) return;
    done = 1;

    struct idtr_t idtr;
    __asm__ volatile("sidt %0" : "=m"(idtr));

    const struct idt_gate_t* idt = (const struct idt_gate_t*)(uintptr_t)idtr.base;
    uint64_t vec6  = gate_target(&idt[6]);
    uint64_t vec13 = gate_target(&idt[13]);

    serial_printf("[idt] pre-guard base=%016lx lim=%04x vec6=%016lx vec13=%016lx\n",
                  (unsigned long)idtr.base, idtr.limit,
                  (unsigned long)vec6, (unsigned long)vec13);

    // No mutation here—just diagnostics. Safe on all compilers/options.

    // Read back to show nothing changed (helps debugging):
    __asm__ volatile("sidt %0" : "=m"(idtr));
    idt   = (const struct idt_gate_t*)(uintptr_t)idtr.base;
    vec6  = gate_target(&idt[6]);
    vec13 = gate_target(&idt[13]);
    serial_printf("[idt] post-guard base=%016lx lim=%04x vec6=%016lx vec13=%016lx\n",
                  (unsigned long)idtr.base, idtr.limit,
                  (unsigned long)vec6, (unsigned long)vec13);
}

struct interrupt_frame { uint64_t rip, cs, rflags, rsp, ss; };

static inline void lidt(const struct idtr64 *d) { __asm__ volatile("lidt %0" : : "m"(*d)); }
static inline uint16_t read_cs(void) { uint16_t cs; __asm__ volatile("mov %%cs,%0":"=r"(cs)); return cs; }

static __attribute__((aligned(16))) struct idt_gate64 s_idt[256];
static int s_done = 0;

#if defined(__GNUC__) && (__GNUC__ >= 5)
#  define INTFN __attribute__((interrupt)) __attribute__((target("general-regs-only")))
#else
#  define INTFN __attribute__((interrupt))
#endif

static void (*g_ud_real)(void*) = 0;
static void (*g_gp_real)(void*, uint64_t) = 0;

/* Minimal, non-faulting stubs */
INTFN static void guard_noerr(struct interrupt_frame* f) { (void)f; }
INTFN static void guard_err(struct interrupt_frame* f, uint64_t ec) { (void)f; (void)ec; }
INTFN static void guard_ud_mux(struct interrupt_frame* f) {
    if (g_ud_real) { g_ud_real(f); return; }
    guard_noerr(f);
}
INTFN static void guard_gp_mux(struct interrupt_frame* f, uint64_t ec) {
    if (g_gp_real) { g_gp_real(f, ec); return; }
    guard_err(f, ec);
}

static inline void gate_set(struct idt_gate64* g, uint16_t cs, uint64_t off, uint8_t ist)
{
    g->off0 = (uint16_t)(off & 0xFFFF);
    g->off1 = (uint16_t)((off >> 16) & 0xFFFF);
    g->off2 = (uint32_t)((off >> 32) & 0xFFFFFFFF);
    g->sel = cs;
    g->ist = (uint8_t)(ist & 0x7);
    g->type_attr = 0x8E;   // P=1 | DPL=0 | Type=0xE (64-bit interrupt gate)
    g->zero = 0;
}

void idt_guard_install_real_handlers(void (*ud_noerr)(void*), void (*gp_err)(void*, uint64_t))
{
    g_ud_real = ud_noerr;
    g_gp_real = gp_err;
}
