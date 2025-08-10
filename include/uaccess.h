#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define USER_TOP   0x00007FFFFFFFFFFFULL
#define HIGH_MASK  0xFFFF000000000000ULL

static inline bool is_canonical_u64(uint64_t x) {
    uint64_t top = x >> 48;
    return (top == 0x0000ULL) || (top == 0xFFFFULL);
}

static inline bool is_user_addr(uint64_t x) {
    return is_canonical_u64(x) && x <= USER_TOP;
}

static inline bool range_add_ok(uint64_t a, size_t n) {
    if (n == 0) return true;
    uint64_t end = a + (n - 1U);
    return end >= a; /* no wrap */
}

__attribute__((weak))
bool range_is_mapped_user(uint64_t start, size_t len) {
    (void)start; (void)len;
    return true;
}

#define CANONICAL_GUARD(p) do { \
    uintptr_t __p = (uintptr_t)(p); \
    if ((__p & HIGH_MASK) == HIGH_MASK && __p <= 0xFFFFFFFFFFFFFFFFULL) { \
        __kernel_panic_noncanonical(__p, __FILE__, __LINE__); \
    } \
} while (0)

void __kernel_panic_noncanonical(uint64_t addr, const char* file, int line);

int copy_from_user(void *dst, const void *user_src, size_t n);
int copy_to_user(void *user_dst, const void *src, size_t n);
