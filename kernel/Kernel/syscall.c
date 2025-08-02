// syscall.c
#include "syscall.h"
#include "../Task/thread.h"
#include "elf.h"
#include "../arch/CPU/smp.h"
#include <stdint.h>

// Allow C linkage if included in a C++ build (for linker sanity)
#ifdef __cplusplus
extern "C" {
#endif

// Example system call numbers (should match syscall.h)
#ifndef SYS_YIELD
#define SYS_YIELD      0
#endif
#ifndef SYS_WRITE_VGA
#define SYS_WRITE_VGA  1
#endif

// --- System Call: Write to VGA text buffer at row 6 ---
uint64_t sys_write_vga(const char *s) {
    volatile uint16_t *vga = (uint16_t *)0xB8000 + 80 * 6; // row 6
    while (*s) {
        *vga++ = (0x0F << 8) | (uint8_t)(*s++);
    }
    return 0;
}

// --- System call dispatcher ---
uint64_t syscall_handle(uint64_t num, uint64_t arg1, uint64_t arg2, uint64_t arg3) {
    (void)arg2; (void)arg3;
    switch (num) {
    case SYS_YIELD:
        thread_yield();
        return 0;
    case SYS_WRITE_VGA:
        return sys_write_vga((const char *)arg1);
    case SYS_FORK: {
        thread_t *cur = current_cpu[smp_cpu_index()];
        thread_t *child = thread_create(cur->func);
        return child ? (uint64_t)child->id : (uint64_t)-1;
    }
    case SYS_EXEC: {
        void *entry = elf_load((const void *)arg1);
        return entry ? (uint64_t)entry : (uint64_t)-1;
    }
    case SYS_SBRK: {
        extern uint8_t _end;
        static uint8_t *brk = &_end;
        uint8_t *old = brk;
        brk += arg1;
        return (uint64_t)old;
    }
    default:
        return (uint64_t)-1;
    }
}

// --- ISR handler for int 0x80 ---
// Must be globally visible (no static), signature must match asm stub
void isr_syscall_handler(void) {
    uint64_t num, a1, a2, a3, ret;
    // Extract syscall arguments (SysV calling convention)
    asm volatile("mov %%rax, %0" : "=r"(num));
    asm volatile("mov %%rdi, %0" : "=r"(a1));
    asm volatile("mov %%rsi, %0" : "=r"(a2));
    asm volatile("mov %%rdx, %0" : "=r"(a3));
    ret = syscall_handle(num, a1, a2, a3);
    asm volatile("mov %0, %%rax" :: "r"(ret));
}

#ifdef __cplusplus
}
#endif
