#pragma once
#include <stdint.h>

// This struct must match your assembly push order!
struct isr_context {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rdi, rsi, rbp, rbx, rdx, rcx, rax;
    uint64_t int_no;
    uint64_t error_code;
    uint64_t rip, cs, rflags, rsp, ss;
    uint64_t cr2;
};
