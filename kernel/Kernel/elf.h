#pragma once
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

enum syscall_num {
    SYS_YIELD         = 0,
    SYS_WRITE_VGA     = 1,
    SYS_FORK          = 2,
    SYS_EXEC          = 3,
    SYS_SBRK          = 4,
    SYS_CLOCK_GETTIME = 5,
    SYS_VM_ALLOCATE   = 6,
    // Add more as needed
};

// Main syscall dispatcher (in syscall.c)
uint64_t syscall_handle(uint64_t num, uint64_t arg1, uint64_t arg2, uint64_t arg3);

// Kernel helper APIs
struct timespec {
    long tv_sec;
    long tv_nsec;
};
int kernel_clock_gettime(int clk_id, struct timespec *tp);
void *kernel_vm_allocate(uint64_t size);

#ifdef __cplusplus
}
#endif
