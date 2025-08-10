#include <stdint.h>
#include "uaccess.h"

extern void kprintf(const char *fmt, ...);

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

void isr_page_fault(regs_t* r) {
    uint64_t cr2 = read_cr2();
    kprintf("!!!! #PF err=%llx CR2=%016llx RIP=%016llx CS=%04llx RFLAGS=%016llx\n",
            (unsigned long long)r->err, (unsigned long long)cr2,
            (unsigned long long)r->rip, (unsigned long long)r->cs,
            (unsigned long long)r->rflags);
    if (!is_canonical_u64(cr2)) {
        kprintf("[pf] NON-CANONICAL CR2 — likely sign-extended 32-bit pointer\n");
    }
}
