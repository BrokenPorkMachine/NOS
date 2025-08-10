#pragma once
#include "uaccess.h"
#include "abi.h"

static inline int abi_get_user_ptr_u64(const uint64_t raw, uintptr_t *out) {
    uintptr_t p = (uintptr_t)raw;
    if (!is_user_addr(p)) return -14;
    *out = p;
    return 0;
}
