#pragma once
#include <stdint.h>

// This struct must match your register push order in assembly!
struct isr_context {
    // General-purpose registers (saved by the stub)
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rdi, rsi, rbp, rbx, rdx, rcx, rax;
    // Interrupt number and error code (always present for uniformity)
    uint64_t int_no;
    uint64_t error_code;
    // Automatically-pushed by CPU on interrupt/exception
    uint64_t rip, cs, rflags, rsp, ss;
    // CR2: present for page faults (must be set in stub)
    uint64_t cr2;
};
