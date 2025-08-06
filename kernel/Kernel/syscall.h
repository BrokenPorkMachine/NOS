#pragma once
#include <stdint.h>

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

uint64_t syscall_handle(uint64_t num, uint64_t arg1, uint64_t arg2, uint64_t arg3);

#ifdef __cplusplus
}
#endif
