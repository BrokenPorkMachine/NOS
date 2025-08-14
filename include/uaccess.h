#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define USER_TOP   0x00007FFFFFFFFFFFULL
#define HIGH_MASK  0xFFFF000000000000ULL

#define KERNEL_BASE 0xFFFF800000000000ULL
#define NOSM_BASE   0xFFFF900000000000ULL
#define MMIO_BASE   0xFFFFC00000000000ULL

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

static inline bool user_ptr_valid(const void *p, size_t n) {
    uintptr_t a = (uintptr_t)p;
    return range_add_ok(a, n) && is_user_addr(a) && range_is_mapped_user(a, n);
}

/*
 * Guard against non-canonical addresses before they make it to the MMU.
 * A pointer is considered valid if its high bits form either the low or high
 * canonical pattern (all zeros or all ones).  Previously the macro treated any
 * pointer with the high bits set (e.g. kernel addresses) as non-canonical,
 * which caused legitimate addresses to be flagged and still allowed truly
 * malformed pointers through.  This version defers to is_canonical_u64() for
 * the actual check, trapping only when the pointer is outside the canonical
 * range.
 */
#define CANONICAL_GUARD(p) do { \
    uintptr_t __p = (uintptr_t)(p); \
    if (!is_canonical_u64(__p)) { \
        __kernel_panic_noncanonical(__p, __FILE__, __LINE__); \
    } \
} while (0)

void __kernel_panic_noncanonical(uint64_t addr, const char* file, int line);

int copy_from_user(void *dst, const void *user_src, size_t n);
int copy_to_user(void *user_dst, const void *src, size_t n);
