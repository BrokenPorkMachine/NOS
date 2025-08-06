#pragma once
#include <stdint.h>

// This struct mirrors the stack layout produced by the ISR stubs in
// arch/IDT/isr_stub.asm.  See that file for the exact push order.
struct isr_context {
    uint64_t cr2;
    uint64_t error_code;
    uint64_t int_no;

    uint64_t rax, rcx, rdx, rbx, rbp, rsi, rdi;
    uint64_t r8, r9, r10, r11, r12, r13, r14, r15;

    uint64_t rip, cs, rflags, rsp, ss;
};
