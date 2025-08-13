#pragma once
#include <stdint.h>

#define SEL(idx, rpl)   (uint16_t)(((idx) << 3) | ((rpl) & 3))  /* TI=0 -> GDT */
enum {
    KCODE = SEL(1, 0),   /* 0x08 */
    KDATA = SEL(2, 0),   /* 0x10 */
    UCODE = SEL(3, 3),   /* 0x1b */
    UDATA = SEL(4, 3),   /* 0x23 */
};

static inline uint16_t sel16(uint64_t s){ return (uint16_t)(s & 0xffff); }

void assert_gdt_selector(uint16_t sel, const char* where);
