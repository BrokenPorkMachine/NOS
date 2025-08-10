#pragma once
#include <stddef.h>
#include <stdint.h>

static inline int __nitros_is_canonical_ptr(const void *p) {
    uintptr_t x = (uintptr_t)p;
    return ((x >> 48) == 0) || ((x >> 48) == 0xFFFF);
}

// Bounded strlen that refuses non-canonical pointers.
static inline size_t __nitros_safe_strnlen(const char *s, size_t max) {
    if (!s || !__nitros_is_canonical_ptr(s)) return 0;
    size_t n = 0;
    while (n < max) { char c = s[n]; if (!c) break; n++; }
    return n;
}
