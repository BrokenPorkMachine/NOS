#include <stdint.h>
#include "uaccess.h"
#include "symbols.h"

extern void kprintf(const char *fmt, ...);
extern void *memcpy(void *dst, const void *src, size_t n);

typedef struct {
    uint64_t rip, cs, rflags, rsp, ss;
    uint64_t rax, rbx, rcx, rdx, rsi, rdi, rbp, r8, r9, r10, r11, r12, r13, r14, r15;
    uint64_t err;
} regs_t;

static inline uint64_t read_cr2(void) {
    uint64_t v;
    __asm__ __volatile__("mov %%cr2,%0" : "=r"(v));
    return v;
}

static void decode_pf_err(uint64_t err) {
    kprintf("[pf] %s %s %s%s%s\n",
            (err & 1) ? "present" : "non-present",
            (err & 2) ? "write" : "read",
            (err & 4) ? "user" : "kernel",
            (err & 8) ? " rsvd" : "",
            (err & 16) ? " exec" : "");
}

static void dump_rip(uint64_t rip) {
    uint8_t bytes[16];
    if (is_user_addr(rip)) {
        if (copy_from_user(bytes, (const void*)rip, sizeof(bytes)) != 0)
            return;
    } else {
        memcpy(bytes, (const void*)rip, sizeof(bytes));
    }
    kprintf("[pf] code @%016llx:", (unsigned long long)rip);
    for (int i = 0; i < (int)sizeof(bytes); ++i)
        kprintf(" %02x", bytes[i]);
    kprintf("\n");
}

void isr_page_fault(regs_t* r) {
    uint64_t cr2 = read_cr2();
    kprintf("!!!! #PF err=%llx CR2=%016llx RIP=%016llx CS=%04llx RFLAGS=%016llx\n",
            (unsigned long long)r->err, (unsigned long long)cr2,
            (unsigned long long)r->rip, (unsigned long long)r->cs,
            (unsigned long long)r->rflags);
    decode_pf_err(r->err);
    uintptr_t off = 0;
    const char *mod = symbols_lookup(r->rip, &off);
    if (mod)
        kprintf("[pf] fault in %s+0x%llx\n", mod, (unsigned long long)off);
    if (!is_canonical_u64(cr2)) {
        kprintf("[pf] NON-CANONICAL CR2 — likely sign-extended 32-bit pointer\n");
    }
    dump_rip(r->rip);
}
