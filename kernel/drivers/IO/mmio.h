#pragma once
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------------
 * Barriers
 *  - mmio_cbarrier(): compiler-only barrier (prevents reordering by the compiler)
 *  - mmio_wmb/rmb/mb: CPU fences; on x86 they’re mostly conservative but ok
 * -------------------------------------------------------------------- */
#define mmio_cbarrier() __asm__ volatile ("" ::: "memory")

#if defined(__x86_64__) || defined(__i386__)
static inline void mmio_wmb(void) { __asm__ volatile ("" ::: "memory"); }  /* stores not reordered wrt MMIO writes on x86 */
static inline void mmio_rmb(void) { __asm__ volatile ("" ::: "memory"); }
static inline void mmio_mb(void)  { __asm__ volatile ("" ::: "memory"); }
#else
/* Portable fallbacks: full seq_cst fences (fine for most kernels) */
static inline void mmio_wmb(void) { __atomic_thread_fence(__ATOMIC_SEQ_CST); }
static inline void mmio_rmb(void) { __atomic_thread_fence(__ATOMIC_SEQ_CST); }
static inline void mmio_mb(void)  { __atomic_thread_fence(__ATOMIC_SEQ_CST); }
#endif

/* ----------------------------------------------------------------------
 * Core typed MMIO accessors
 *  - The basic read/write forms include a compiler barrier after access.
 *  - The *_sync write forms force a read-back to flush posted writes.
 * -------------------------------------------------------------------- */
#define MMIO_RW(width, type)                                                           \
    static inline void mmio_write##width(uintptr_t addr, type val) {                   \
        *(volatile type *)(addr) = val;                                               \
        mmio_cbarrier();                                                              \
    }                                                                                 \
    static inline type mmio_read##width(uintptr_t addr) {                              \
        type v = *(volatile type *)(addr);                                            \
        mmio_cbarrier();                                                              \
        return v;                                                                     \
    }                                                                                 \
    /* Synchronous write: read-back to defeat write posting */                         \
    static inline void mmio_write##width##_sync(uintptr_t addr, type val) {            \
        *(volatile type *)(addr) = val;                                               \
        (void)*(volatile type *)(addr);                                               \
        mmio_cbarrier();                                                              \
    }

MMIO_RW(8,  uint8_t)
MMIO_RW(16, uint16_t)
MMIO_RW(32, uint32_t)
MMIO_RW(64, uint64_t)

#undef MMIO_RW

/* ----------------------------------------------------------------------
 * Convenience helpers
 * -------------------------------------------------------------------- */

/* Compute (base + offset) as uintptr_t for the accessors above. */
static inline uintptr_t mmio_off(const void *base, size_t offset) {
    return (uintptr_t)base + (uintptr_t)offset;
}

/* Bit helpers (read-modify-write). Use *_sync if the device requires it. */
static inline void mmio_set_bits32(uintptr_t addr, uint32_t mask) {
    mmio_write32(addr, mmio_read32(addr) | mask);
}
static inline void mmio_clear_bits32(uintptr_t addr, uint32_t mask) {
    mmio_write32(addr, mmio_read32(addr) & ~mask);
}
static inline void mmio_update_bits32(uintptr_t addr, uint32_t mask, uint32_t value) {
    uint32_t v = mmio_read32(addr);
    v = (v & ~mask) | (value & mask);
    mmio_write32(addr, v);
}

/* 64-bit variants (if your device registers are 64-bit wide) */
static inline void mmio_set_bits64(uintptr_t addr, uint64_t mask) {
    mmio_write64(addr, mmio_read64(addr) | mask);
}
static inline void mmio_clear_bits64(uintptr_t addr, uint64_t mask) {
    mmio_write64(addr, mmio_read64(addr) & ~mask);
}
static inline void mmio_update_bits64(uintptr_t addr, uint64_t mask, uint64_t value) {
    uint64_t v = mmio_read64(addr);
    v = (v & ~mask) | (value & mask);
    mmio_write64(addr, v);
}

/* Optional: explicit posted-write flush against a known safe register. */
static inline void mmio_flush_posted32(uintptr_t flush_addr) {
    (void)mmio_read32(flush_addr);
}
static inline void mmio_flush_posted64(uintptr_t flush_addr) {
    (void)mmio_read64(flush_addr);
}

#ifdef __cplusplus
} /* extern "C" */
#endif
