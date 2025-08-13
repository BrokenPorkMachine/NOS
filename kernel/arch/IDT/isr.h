#pragma once
#include <stdint.h>

/* Trap frame layout as pushed by your ASM stubs (adjust if yours differs).
   Keep packed for exact layout. */
struct __attribute__((packed)) isr_context {
    /* If you push regs in ASM, list them here in order. For the minimal stubs above,
       we didn't push general regs for #UD; timer pushes rax, rcx, rdx before call. */
    /* ... your saved regs if any ... */

    /* iret frame (CPU-pushed on interrupt/exception entry) */
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
};

/* Your C-level ISR handlers, if you have them */
void isr_timer_handler(struct isr_context *ctx);

/* Provided by your scheduler */
uint64_t *schedule_from_isr(const void *frame);
