#ifndef MMIO_H
#define MMIO_H

#include <stdint.h>
#include <stddef.h>

static inline void mmio_write8(uintptr_t addr, uint8_t val) {
    *(volatile uint8_t *)addr = val;
    asm volatile("" ::: "memory");
}

static inline uint8_t mmio_read8(uintptr_t addr) {
    uint8_t ret = *(volatile uint8_t *)addr;
    asm volatile("" ::: "memory");
    return ret;
}

static inline void mmio_write16(uintptr_t addr, uint16_t val) {
    *(volatile uint16_t *)addr = val;
    asm volatile("" ::: "memory");
}

static inline uint16_t mmio_read16(uintptr_t addr) {
    uint16_t ret = *(volatile uint16_t *)addr;
    asm volatile("" ::: "memory");
    return ret;
}

static inline void mmio_write32(uintptr_t addr, uint32_t val) {
    *(volatile uint32_t *)addr = val;
    asm volatile("" ::: "memory");
}

static inline uint32_t mmio_read32(uintptr_t addr) {
    uint32_t ret = *(volatile uint32_t *)addr;
    asm volatile("" ::: "memory");
    return ret;
}

static inline void mmio_write64(uintptr_t addr, uint64_t val) {
    *(volatile uint64_t *)addr = val;
    asm volatile("" ::: "memory");
}

static inline uint64_t mmio_read64(uintptr_t addr) {
    uint64_t ret = *(volatile uint64_t *)addr;
    asm volatile("" ::: "memory");
    return ret;
}

#endif // MMIO_H
