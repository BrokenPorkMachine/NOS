#pragma once
#include <stdint.h>

/*
 * Interrupt/Exception context frame
 *
 * Stack layout (lowest address at top; RDI points here in handlers):
 *
 *   +0x00  uint64_t int_no       ; synthetic (pushed by stub)
 *   +0x08  uint64_t error_code   ; synthetic (CPU error code copied or 0)
 *   +0x10  uint64_t cr2          ; synthetic (CR2 for #PF, else 0)
 *
 *   +0x18  uint64_t rax          ; saved by stub (PUSH_REGS)
 *   +0x20  uint64_t rcx
 *   +0x28  uint64_t rdx
 *   +0x30  uint64_t rbx
 *   +0x38  uint64_t rbp
 *   +0x40  uint64_t rsi
 *   +0x48  uint64_t rdi
 *   +0x50  uint64_t r8
 *   +0x58  uint64_t r9
 *   +0x60  uint64_t r10
 *   +0x68  uint64_t r11
 *   +0x70  uint64_t r12
 *   +0x78  uint64_t r13
 *   +0x80  uint64_t r14
 *   +0x88  uint64_t r15
 *
 *   +0x90  uint64_t rip          ; from CPU hardware frame
 *   +0x98  uint64_t cs
 *   +0xA0  uint64_t rflags
 *   +0xA8  uint64_t rsp          ; ONLY if CPL changed (user->kernel). Otherwise undefined.
 *   +0xB0  uint64_t ss           ; ONLY if CPL changed (user->kernel). Otherwise undefined.
 *
 * Notes:
 * - For exceptions that push a hardware error code (e.g., #PF/#GP/#DF/#TS/#NP/#SS/#AC),
 *   the stub copies that value into error_code and discards the CPU-pushed slot before iretq.
 *   Handlers should always read error_code from this struct and ignore any hardware slot.
 * - cr2 is meaningful only for #PF; for other vectors it is zero.
 * - To test if the trap came from user mode, check (cs & 3) == 3.
 */

struct isr_context {
    /* synthetic header pushed by the stub */
    uint64_t int_no;
    uint64_t error_code;
    uint64_t cr2;

    /* general-purpose registers (order matches POP_REGS in the stubs) */
    uint64_t rax, rcx, rdx, rbx, rbp, rsi, rdi;
    uint64_t r8, r9, r10, r11, r12, r13, r14, r15;

    /* hardware frame (from CPU) */
    uint64_t rip, cs, rflags, rsp, ss; /* rsp/ss valid only on CPL change */
} __attribute__((packed));

/* Helper: true if trap originated from ring 3 (user mode) */
static inline int isr_from_user(const struct isr_context *ctx) {
    return (int)((ctx->cs & 3u) == 3u);
}

/* Page-fault error-code bit helpers (Intel SDM Vol.3) */
enum {
    PF_ERR_P    = 1u << 0,  /* 0: non-present, 1: protection violation */
    PF_ERR_WR   = 1u << 1,  /* 0: read, 1: write */
    PF_ERR_US   = 1u << 2,  /* 0: supervisor, 1: user */
    PF_ERR_RSVD = 1u << 3,  /* reserved bit set in paging structure */
    PF_ERR_ID   = 1u << 4,  /* 1: instruction fetch (if supported) */
    PF_ERR_PK   = 1u << 5,  /* protection key violation */
    PF_ERR_SS   = 1u << 6   /* shadow stack (CET) */
};
