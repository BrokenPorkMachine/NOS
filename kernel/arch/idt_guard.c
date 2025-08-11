#include <stdint.h>
#include <stddef.h>

#ifndef IDT_GUARD_DANGER_MAX
// Anything below 1 MiB is considered suspect (OVMF stubs, real-mode shadows, etc).
#define IDT_GUARD_DANGER_MAX 0x00100000ULL
#endif

// --- IDTR / IDT structures (x86-64) -----------------------------------------
struct __attribute__((packed)) idtr64 {
    uint16_t limit;
    uint64_t base;
};

struct __attribute__((packed)) idt_gate64 {
    uint16_t off0;
    uint16_t sel;
    uint8_t  ist;
    uint8_t  type_attr;  // 0x8E = present 64-bit interrupt gate
    uint16_t off1;
    uint32_t off2;
    uint32_t zero;
};

static inline void sidt(struct idtr64 *d) { __asm__ volatile("sidt %0" : "=m"(*d)); }
static inline void lidt(const struct idtr64 *d) { __asm__ volatile("lidt %0" : : "m"(*d)); }
static inline uint16_t read_cs(void) { uint16_t cs; __asm__ volatile("mov %%cs,%0":"=r"(cs)); return cs; }

static inline uint64_t gate_get_off(const struct idt_gate64* g) {
    return ((uint64_t)g->off0) | ((uint64_t)g->off1 << 16) | ((uint64_t)g->off2 << 32);
}
static inline void gate_set_off(struct idt_gate64* g, uint64_t off) {
    g->off0 = (uint16_t)(off & 0xFFFF);
    g->off1 = (uint16_t)((off >> 16) & 0xFFFF);
    g->off2 = (uint32_t)((off >> 32) & 0xFFFFFFFF);
}

// Simple memcpy/memset (no libc)
static void kmemcpy(void* dst, const void* src, size_t n) {
    uint8_t* d = (uint8_t*)dst; const uint8_t* s = (const uint8_t*)src;
    while (n--) *d++ = *s++;
}
static void kmemset(void* dst, int v, size_t n) {
    uint8_t* d = (uint8_t*)dst; uint8_t b = (uint8_t)v;
    while (n--) *d++ = b;
}

// --- Guard stubs -------------------------------------------------------------
// Use the "interrupt" attribute so the compiler emits an IRETQ on return.
// We intentionally *return* (instead of HLT/panic) so spurious vectors don't deadlock
// bring-up. Replace later with real handlers via idt_guard_install_real_handlers().
struct interrupt_frame {
    uint64_t rip, cs, rflags, rsp, ss;
};

__attribute__((interrupt)) static void guard_noerr(struct interrupt_frame* frame) {
    (void)frame;  // drop it; just return
}
__attribute__((interrupt)) static void guard_err(struct interrupt_frame* frame, uint64_t ec) {
    (void)frame; (void)ec; // drop it; just return
}

// Pointers used for final handler takeover (optional).
static void (*g_ud_real)(void*) = 0;
static void (*g_gp_real)(void*, uint64_t) = 0;

// Small trampoline pairs that either call real handlers (when installed) or fall back.
__attribute__((interrupt)) static void guard_ud_mux(struct interrupt_frame* f) {
    if (g_ud_real) { g_ud_real(f); return; } guard_noerr(f);
}
__attribute__((interrupt)) static void guard_gp_mux(struct interrupt_frame* f, uint64_t ec) {
    if (g_gp_real) { g_gp_real(f, ec); return; } guard_err(f, ec);
}

// Decide if a vector pushes an error code (architectural)
static inline int vec_has_error_code(int vec) {
    switch (vec) {
        case 8:  // DF
        case 10: // TS
        case 11: // NP
        case 12: // SS
        case 13: // GP
        case 14: // PF
        case 17: // AC
        case 30: // SX
            return 1;
        default: return 0;
    }
}

// IDT clone buffer and state
static __attribute__((aligned(16))) uint8_t s_idt_clone[4096];
static int s_done = 0;

static void patch_gate(struct idt_gate64* g, int vec, uint16_t cs)
{
    // Always normalize type/sel; if handler points low, repoint to guard.
    uint64_t off = gate_get_off(g);

    // Normalize descriptor
    g->sel = cs;
    // Present | DPL=0 | Type=0xE (interrupt gate) | 64-bit gate bit comes from type variant
    g->type_attr = 0x8E;
    g->ist &= 0x7; // keep IST bits if any

    if (off < IDT_GUARD_DANGER_MAX) {
        // Known-bad/legacy target: replace with guard stubs.
        if (vec == 6) {
            gate_set_off(g, (uint64_t)(void*)&guard_ud_mux);
        } else if (vec == 13) {
            gate_set_off(g, (uint64_t)(void*)&guard_gp_mux);
        } else {
            if (vec_has_error_code(vec))
                gate_set_off(g, (uint64_t)(void*)&guard_err);
            else
                gate_set_off(g, (uint64_t)(void*)&guard_noerr);
        }
    }
}

void idt_guard_init_once(void)
{
    if (s_done) return;

    struct idtr64 cur; sidt(&cur);
    size_t sz = (size_t)cur.limit + 1;
    if (sz == 0 || sz > sizeof(s_idt_clone)) sz = sizeof(s_idt_clone);

    // Clone current IDT.
    kmemcpy(s_idt_clone, (const void*)cur.base, sz);

    // Patch suspicious entries.
    uint16_t cs = read_cs();
    size_t entries = sz / sizeof(struct idt_gate64);
    if (entries > 256) entries = 256;

    for (size_t i = 0; i < entries; ++i) {
        struct idt_gate64* g = (struct idt_gate64*)(s_idt_clone + i * sizeof(struct idt_gate64));
        patch_gate(g, (int)i, cs);
    }

    // Load the sanitized clone.
    struct idtr64 newd = { .limit = (uint16_t)(entries*sizeof(struct idt_gate64) - 1),
                           .base  = (uint64_t)(void*)s_idt_clone };
    lidt(&newd);

    s_done = 1;
}

void idt_guard_install_real_handlers(void (*ud_noerr)(void*), void (*gp_err)(void*, uint64_t))
{
    g_ud_real = ud_noerr;
    g_gp_real = gp_err;
    // Keep using the cloned IDT; mux stubs will forward from now on.
}
