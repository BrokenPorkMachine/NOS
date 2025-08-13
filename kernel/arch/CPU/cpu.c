#include <stdint.h>

/* If you have your own cpuid helpers in ../../../include/cpuid.h, you can
   drop the inline asm below and call those instead. */
static inline void cpuid2(uint32_t leaf, uint32_t subleaf,
                          uint32_t *a, uint32_t *b, uint32_t *c, uint32_t *d)
{
    uint32_t ra, rb, rc, rd;
    __asm__ volatile("cpuid"
                     : "=a"(ra), "=b"(rb), "=c"(rc), "=d"(rd)
                     : "a"(leaf), "c"(subleaf));

    if (a) {
        *a = ra;
    }
    if (b) {
        *b = rb;
    }
    if (c) {
        *c = rc;
    }
    if (d) {
        *d = rd;
    }
}

static inline uint32_t cpuid_max_basic(void) {
    uint32_t a; cpuid2(0, 0, &a, 0, 0, 0); return a;
}
static inline uint32_t cpuid_max_ext(void) {
    uint32_t a; cpuid2(0x80000000u, 0, &a, 0, 0, 0); return a;
}

static uint32_t detect_via_topology(uint32_t leaf)
{
    /* CPUID.(leaf, subleaf):
       ECX[15:8] level type: 1=SMT, 2=Core; EBX = logical processors at this level and below.
       Iterate subleafs until EBX==0. We want EBX for level type Core. */
    uint32_t sub = 0, logical_pkg = 0;
    for (;;) {
        uint32_t a, b, c, d;
        cpuid2(leaf, sub, &a, &b, &c, &d);
        if (b == 0) break;            /* no more levels */
        uint32_t level_type = (c >> 8) & 0xff;
        if (level_type == 2)          /* core level => EBX = logical per package */
            logical_pkg = b;
        sub++;
    }
    return logical_pkg;
}

uint32_t cpu_detect_logical_count(void)
{
    static uint32_t cached = 0;
    if (cached) return cached;

    uint32_t max_basic = cpuid_max_basic();
    uint32_t max_ext   = cpuid_max_ext();

    uint32_t logical = 0;

    /* Prefer modern topology leaves if present */
    if (max_basic >= 0x1F) {
        logical = detect_via_topology(0x1F);
    }
    if (!logical && max_basic >= 0x0B) {
        logical = detect_via_topology(0x0B);
    }

    /* Legacy fallback: CPUID.1:EBX[23:16] = logical processors per package */
    if (!logical && max_basic >= 0x01) {
        uint32_t a, b, c, d;
        cpuid2(0x01, 0, &a, &b, &c, &d);
        logical = (b >> 16) & 0xff;
    }

    /* AMD hint: CPUID.80000008:ECX[7:0] = cores per package - 1.
       If legacy logical looks bogus (0 or 1 with HT support), try to make a better guess. */
    if (logical == 0 && max_ext >= 0x80000008u) {
        uint32_t a, b, c, d;
        cpuid2(0x80000008u, 0, &a, &b, &c, &d);
        uint32_t cores = (c & 0xffu) + 1u;   /* cores per package */
        if (cores) logical = cores;          /* assume 1 thread/core if we don't know SMT */
    }

    if (logical == 0) logical = 1; /* final safety */

    cached = logical;
    return cached;
}
