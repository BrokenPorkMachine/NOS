#ifndef MMIO_H
#define MMIO_H

#include <stddef.h>
#include <stdint.h>

/*
 * Minimal memory barrier used for MMIO accesses.  The empty "asm" ensures the
 * compiler does not reorder accesses around the barrier.  Keeping it in a
 * macro allows reuse across all helpers below.
 */
#define mmio_barrier() asm volatile("" ::: "memory")

/*
 * Generate typed MMIO read/write helpers.  Each helper issues the memory
 * barrier after the access to prevent the compiler from reordering operations
 * and to make intent explicit.
 */
#define MMIO_RW(width, type)                                                   \
    static inline void mmio_write##width(uintptr_t addr, type val) {           \
        *(volatile type *)addr = val;                                          \
        mmio_barrier();                                                        \
    }                                                                          \
    static inline type mmio_read##width(uintptr_t addr) {                      \
        type ret = *(volatile type *)addr;                                     \
        mmio_barrier();                                                        \
        return ret;                                                            \
    }

MMIO_RW(8, uint8_t)
MMIO_RW(16, uint16_t)
MMIO_RW(32, uint32_t)
MMIO_RW(64, uint64_t)

#undef MMIO_RW
#undef mmio_barrier

#endif // MMIO_H
