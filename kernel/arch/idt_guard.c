#include <stdint.h>
#include <stddef.h>

struct __attribute__((packed)) idtr64 {
    uint16_t limit;
    uint64_t base;
};
struct __attribute__((packed)) idt_gate64 {
    uint16_t off0;
    uint16_t sel;
    uint8_t  ist;
    uint8_t  type_attr;
    uint16_t off1;
    uint32_t off2;
    uint32_t zero;
};
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

void idt_guard_init_once(void)
{
    if (s_done) return;

    uint16_t cs = read_cs();

    // Exceptions 0..31: give them safe handlers. Others: benign sink.
    for (int i = 0; i < 256; ++i) {
        uint64_t target = (uint64_t)(void*)&guard_noerr;
        int has_err = 0;

        switch (i) {
            case 6:  target = (uint64_t)(void*)&guard_ud_mux; break;   // #UD
            case 13: target = (uint64_t)(void*)&guard_gp_mux; has_err = 1; break; // #GP
            case 8: case 10: case 11: case 12: case 14: case 17: case 30: has_err = 1; break;
            default: break;
        }
        if (has_err) target = (target == (uint64_t)(void*)&guard_noerr)
                                ? (uint64_t)(void*)&guard_err : target;
        gate_set(&s_idt[i], cs, target, 0);
    }

    struct idtr64 idt = {
        .limit = (uint16_t)(sizeof(s_idt) - 1),
        .base  = (uint64_t)(void*)s_idt
    };
    lidt(&idt);
    s_done = 1;
}

void idt_guard_install_real_handlers(void (*ud_noerr)(void*), void (*gp_err)(void*, uint64_t))
{
    g_ud_real = ud_noerr;
    g_gp_real = gp_err;
}
