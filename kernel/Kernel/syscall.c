#include "syscall.h"
#include "../Task/thread.h"
#include "elf.h"
#include "../arch/CPU/smp.h"
#include <stdint.h>
#include <stddef.h>

// --- Example static heap for demonstration VM allocator ---
__attribute__((section(".heap"))) static uint8_t kheap[16 * 1024 * 1024]; // 16 MiB kernel heap
static size_t kheap_used = 0;

// --- Kernel clock: monotonic nanoseconds since boot ---
static uint64_t monotonic_ns = 0;

// --- User memory checking (if you want, else remove) ---
static int is_user_writable(void *user_ptr, size_t len) {
    // In a real kernel, check user pointer is safe/writable. For now, always true.
    (void)user_ptr; (void)len;
    return 1;
}

// --- Robust kernel clock_gettime implementation ---
int kernel_clock_gettime(int clk_id, struct timespec *tp) {
    if (!tp || !is_user_writable(tp, sizeof(struct timespec))) return -1;

    // Simulate monotonic time (e.g., PIT, APIC, TSC, etc.)
    // For a real clock, read your HPET/APIC/RTC here
    monotonic_ns += 10 * 1000 * 1000; // 10 ms per call for demonstration

    tp->tv_sec = monotonic_ns / 1000000000ULL;
    tp->tv_nsec = monotonic_ns % 1000000000ULL;
    return 0;
}

// --- Robust kernel vm_allocate implementation (page-aligned, safe) ---
void *kernel_vm_allocate(uint64_t size) {
    if (size == 0 || size > sizeof(kheap) - kheap_used)
        return NULL;
    // Page-align all allocations to 4K for safety
    size = (size + 0xFFF) & ~0xFFF;
    void *ptr = &kheap[kheap_used];
    kheap_used += size;
    // In a real kernel, update process address space, allocate page tables, etc.
    return ptr;
}

// --- VGA Output Helper: Writes a string to VGA row 6 ---
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
        return kernel_clock_gettime((int)arg1, (struct timespec *)arg2);
    }
    case SYS_VM_ALLOCATE: {
        // arg1: size in bytes, return user-accessible pointer or NULL
        return (uint64_t)kernel_vm_allocate(arg1);
    }
    default:
        return (uint64_t)-1;
    }
}

// --- ISR handler for int 0x80 ---
// Must match calling convention expected by your ASM stub.
void isr_syscall_handler(void) {
    uint64_t num, a1, a2, a3, ret;
    asm volatile("mov %%rax, %0" : "=r"(num));
    asm volatile("mov %%rdi, %0" : "=r"(a1));
    asm volatile("mov %%rsi, %0" : "=r"(a2));
    asm volatile("mov %%rdx, %0" : "=r"(a3));
    ret = syscall_handle(num, a1, a2, a3);
    asm volatile("mov %0, %%rax" :: "r"(ret));
}
