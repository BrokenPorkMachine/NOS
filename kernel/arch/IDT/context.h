#pragma once
#include <stdint.h>

// This struct must match the register push order in isr_stub.asm.
// The assembly stubs push values in the following sequence (from first to
// last): general-purpose registers, the interrupt metadata, and finally the
// CPU-pushed state (RIP, CS, RFLAGS, etc).  To obtain a pointer to this
// structure, the stub simply passes the current RSP after pushing the CR2
// value.  Consequently, the first fields here correspond to the last values
// pushed.
struct isr_context {
    // CR2 (for page faults), error code and vector number
    uint64_t cr2;
    uint64_t error_code;
    uint64_t int_no;

    // General-purpose registers saved by the stub
    uint64_t rax, rcx, rdx, rbx, rbp, rsi, rdi;
    uint64_t r8, r9, r10, r11, r12, r13, r14, r15;

    // Automatically pushed by the CPU on interrupt/exception entry
    uint64_t rip, cs, rflags, rsp, ss;
};
