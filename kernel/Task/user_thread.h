#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Saved CPU context for a thread. Matches iretq-compatible layout. */
typedef struct cpu_context {
    uint64_t r15, r14, r13, r12;
    uint64_t r11, r10, r9, r8;
    uint64_t rsi, rdi, rdx, rcx;
    uint64_t rbx, rbp, rax;
    uint64_t rip, cs, rflags;
    uint64_t rsp, ss;
} cpu_context;

/* Prepare a context so a later iretq enters user mode. */
void init_user_thread(cpu_context *ctx, uint64_t entry, uint64_t user_stack);

/* Drop to user mode at provided RIP/RSP via iretq. Never returns. */
void switch_to_user(uint64_t rip, uint64_t rsp) __attribute__((noreturn));

#ifdef __cplusplus
}
#endif
