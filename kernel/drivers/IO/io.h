#pragma once
#include <stdint.h>
#include <stddef.h>

/* ---------------- Single-port I/O ---------------- */

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" :: "a"(val), "Nd"(port) : "memory");
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port) : "memory");
    return ret;
}

static inline void outw(uint16_t port, uint16_t val) {
    __asm__ volatile ("outw %0, %1" :: "a"(val), "Nd"(port) : "memory");
}

static inline uint16_t inw(uint16_t port) {
    uint16_t ret;
    __asm__ volatile ("inw %1, %0" : "=a"(ret) : "Nd"(port) : "memory");
    return ret;
}

static inline void outl(uint16_t port, uint32_t val) {
    __asm__ volatile ("outl %0, %1" :: "a"(val), "Nd"(port) : "memory");
}

static inline uint32_t inl(uint16_t port) {
    uint32_t ret;
    __asm__ volatile ("inl %1, %0" : "=a"(ret) : "Nd"(port) : "memory");
    return ret;
}

/* ---------------- Repeated string I/O ----------------
   Note: The CPU reads/writes from [RSI]/[RDI] and auto-increments.
   We mark memory clobbered to prevent the compiler from reordering. */

static inline void outsb(uint16_t port, const void *addr, size_t count) {
    const uint8_t *src = (const uint8_t *)addr;
    __asm__ volatile ("rep outsb"
                      : "+S"(src), "+c"(count)
                      : "d"(port)
                      : "memory");
}

static inline void insb(uint16_t port, void *addr, size_t count) {
    uint8_t *dst = (uint8_t *)addr;
    __asm__ volatile ("rep insb"
                      : "+D"(dst), "+c"(count)
                      : "d"(port)
                      : "memory");
}

static inline void outsw(uint16_t port, const void *addr, size_t count) {
    const uint16_t *src = (const uint16_t *)addr;
    __asm__ volatile ("rep outsw"
                      : "+S"(src), "+c"(count)
                      : "d"(port)
                      : "memory");
}

static inline void insw(uint16_t port, void *addr, size_t count) {
    uint16_t *dst = (uint16_t *)addr;
    __asm__ volatile ("rep insw"
                      : "+D"(dst), "+c"(count)
                      : "d"(port)
                      : "memory");
}

static inline void outsl(uint16_t port, const void *addr, size_t count) {
    const uint32_t *src = (const uint32_t *)addr;
    __asm__ volatile ("rep outsl"
                      : "+S"(src), "+c"(count)
                      : "d"(port)
                      : "memory");
}

static inline void insl(uint16_t port, void *addr, size_t count) {
    uint32_t *dst = (uint32_t *)addr;
    __asm__ volatile ("rep insl"
                      : "+D"(dst), "+c"(count)
                      : "d"(port)
                      : "memory");
}

/* ---------------- Tiny delays / barriers ---------------- */

/* Classic POST-port I/O delay (1 byte to port 0x80). Safe on PC chipset. */
static inline void io_wait(void) {
    __asm__ volatile ("outb %0, $0x80" :: "a"(0) : "memory");
}

/* Compiler barrier around MMIO/PIO sequences (does not serialize the CPU). */
static inline void io_barrier(void) {
    __asm__ volatile ("" ::: "memory");
}
