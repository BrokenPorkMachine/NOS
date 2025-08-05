#pragma once
#include <stdint.h>

struct isr_context {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rdi, rsi, rbp, rbx, rdx, rcx, rax;
    uint64_t int_no;       // Interrupt vector
    uint64_t error_code;   // Valid only for some interrupts
    uint64_t rip, cs, rflags, user_rsp, ss;
};
