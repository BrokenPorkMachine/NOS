#pragma once
#include <stdint.h>

enum syscall_num {
    SYS_YIELD = 0,
    SYS_WRITE_VGA = 1,
};

uint64_t syscall_handle(uint64_t num, uint64_t arg1, uint64_t arg2, uint64_t arg3);
