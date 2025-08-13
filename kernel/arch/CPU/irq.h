#pragma once
#include <stdint.h>

static inline uint64_t read_rflags(void) {
    uint64_t f; __asm__ __volatile__("pushfq; pop %0" : "=r"(f));
    return f;
}
static inline void sti(void) { __asm__ __volatile__("sti" ::: "memory"); }
static inline void cli(void) { __asm__ __volatile__("cli" ::: "memory"); }
