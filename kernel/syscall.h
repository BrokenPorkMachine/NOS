#pragma once
#include <stdint.h>
#include <stddef.h>

typedef struct {
    uint64_t rax;
    uint64_t rcx;
    uint64_t rdx;
    uint64_t rsi;
    uint64_t rdi;
    uint64_t r8;
    uint64_t r9;
    uint64_t r10;
    uint64_t r11;
} syscall_regs_t;

typedef long (*syscall_fn_t)(syscall_regs_t *regs);

int  n2_syscall_register(uint32_t num, syscall_fn_t fn);
void syscalls_init(void);
long isr_syscall_handler(syscall_regs_t *regs);
void devfs_init(void);
