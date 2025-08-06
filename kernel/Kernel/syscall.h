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
    SYS_NITRFS_CREATE     = 32,
    SYS_NITRFS_WRITE      = 33,
    SYS_NITRFS_READ       = 34,
    SYS_NITRFS_DELETE     = 35,
    SYS_NITRFS_RENAME     = 36,
    SYS_NITRFS_LIST       = 37,
    SYS_NITRFS_ACL_ADD    = 38,
    SYS_NITRFS_ACL_REMOVE = 39,
    SYS_NITRFS_ACL_CHECK  = 40,
    SYS_NITRFS_ACL_LIST   = 41,
    SYS_NITRFS_RESIZE     = 42,
    SYS_NITRFS_GET_TS     = 43,    // Add more as needed
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
