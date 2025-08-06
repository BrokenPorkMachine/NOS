#include "syscall.h"
#include "../Task/thread.h"
#include "elf.h"
#include "../arch/CPU/smp.h"
#include <stdint.h>
#include <stddef.h>
#include <time.h>    // For struct timespec if available

// --- Placeholders: Provide these in your kernel implementation ---
int kernel_clock_gettime(int clk_id, struct timespec *tp);
void *kernel_vm_allocate(uint64_t size);

int kernel_clock_gettime(int clk_id, struct timespec *tp) {
    if (!tp) return -1;
    // Example: fake monotonic time
    static uint64_t ticks = 0;
    ticks += 10000000; // nanoseconds (10ms per call)
    tp->tv_sec = ticks / 1000000000ULL;
    tp->tv_nsec = ticks % 1000000000ULL;
    return 0;
}

void *kernel_vm_allocate(uint64_t size) {
    // Implement page allocation, return user pointer or NULL.
    // Example: Just static for bootstrapping
    extern uint8_t _heap[];
    static size_t used = 0;
    void *ptr = _heap + used;
    used += (size + 0xFFF) & ~0xFFF; // page align
    return ptr;
}

// --- VGA Output Helper ---
static uint64_t sys_write_vga(const char *s) {
    volatile uint16_t *vga = (uint16_t *)0xB8000 + 80 * 6;
    while (*s) {
        *vga++ = (0x0F << 8) | (uint8_t)(*s++);
    }
    return 0;
}

// --- Main System Call Dispatcher ---
uint64_t syscall_handle(uint64_t num, uint64_t arg1, uint64_t arg2, uint64_t arg3) {
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
    case SYS_CLOCK_GETTIME: {
        // arg1: clk_id, arg2: struct timespec* (user pointer)
        // Implement this function in your kernel!
        return kernel_clock_gettime((int)arg1, (struct timespec *)arg2);
    }
    case SYS_VM_ALLOCATE: {
        // arg1: size
        // Returns a user-accessible pointer (or 0/NULL on error)
        return (uint64_t)kernel_vm_allocate(arg1);
    }
    default:
        return (uint64_t)-1;
    }
}

// --- ISR handler for int 0x80 ---
void isr_syscall_handler(void) {
    uint64_t num, a1, a2, a3, ret;
    asm volatile("mov %%rax, %0" : "=r"(num));
    asm volatile("mov %%rdi, %0" : "=r"(a1));
    asm volatile("mov %%rsi, %0" : "=r"(a2));
    asm volatile("mov %%rdx, %0" : "=r"(a3));
    ret = syscall_handle(num, a1, a2, a3);
    asm volatile("mov %0, %%rax" :: "r"(ret));
}
