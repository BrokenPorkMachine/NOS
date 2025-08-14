#include <stdint.h>
#include <string.h>
#include "cpu.h"

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

static void identify_microarch(struct cpu_features *info)
{
    const char *name = "Unknown";
    if (!strcmp(info->vendor, "GenuineIntel")) {
        switch (info->model) {
        case 0x4E:
        case 0x5E:
        case 0x8E:
        case 0x9E:
            name = "Intel Skylake";
            break;
        default:
            name = "Intel (unknown)";
            break;
        }
    } else if (!strcmp(info->vendor, "AuthenticAMD")) {
        switch (info->family) {
        case 0x17:
            name = "AMD Zen";
            break;
        case 0x19:
            name = "AMD Zen 3";
            break;
        default:
            name = "AMD (unknown)";
            break;
        }
    }
    strncpy(info->microarch, name, sizeof(info->microarch));
    info->microarch[sizeof(info->microarch) - 1] = '\0';
}

void cpu_detect_features(struct cpu_features *out)
{
    memset(out, 0, sizeof(*out));

    uint32_t a, b, c, d;

    cpuid2(0, 0, &a, &b, &c, &d);
    ((uint32_t *)out->vendor)[0] = b;
    ((uint32_t *)out->vendor)[1] = d;
    ((uint32_t *)out->vendor)[2] = c;
    out->vendor[12] = '\0';

    uint32_t max_basic = cpuid_max_basic();
    uint32_t max_ext = cpuid_max_ext();

    if (max_ext >= 0x80000004u) {
        for (uint32_t i = 0; i < 3; i++) {
            cpuid2(0x80000002u + i, 0, &a, &b, &c, &d);
            memcpy(out->brand + i * 16 + 0, &a, 4);
            memcpy(out->brand + i * 16 + 4, &b, 4);
            memcpy(out->brand + i * 16 + 8, &c, 4);
            memcpy(out->brand + i * 16 + 12, &d, 4);
        }
        out->brand[48] = '\0';
    }

    uint32_t eax1, ebx1, ecx1, edx1;
    cpuid2(1, 0, &eax1, &ebx1, &ecx1, &edx1);
    uint32_t family = (eax1 >> 8) & 0xf;
    uint32_t model = (eax1 >> 4) & 0xf;
    uint32_t stepping = eax1 & 0xf;
    uint32_t ext_family = (eax1 >> 20) & 0xff;
    uint32_t ext_model = (eax1 >> 16) & 0xf;
    if (family == 0xF)
        family += ext_family;
    if (family == 0x6 || family == 0xF)
        model += ext_model << 4;

    out->family = family;
    out->model = model;
    out->stepping = stepping;

    out->mmx   = edx1 & (1u << 23);
    out->sse   = edx1 & (1u << 25);
    out->sse2  = edx1 & (1u << 26);
    out->sse3  = ecx1 & (1u << 0);
    out->ssse3 = ecx1 & (1u << 9);
    out->sse41 = ecx1 & (1u << 19);
    out->sse42 = ecx1 & (1u << 20);
    out->avx   = ecx1 & (1u << 28);
    out->fma   = ecx1 & (1u << 12);
    out->vt_x  = ecx1 & (1u << 5);

    if (max_basic >= 7) {
        cpuid2(7, 0, &a, &b, &c, &d);
        out->bmi1    = b & (1u << 3);
        out->bmi2    = b & (1u << 8);
        out->avx2    = b & (1u << 5);
        out->avx512f = b & (1u << 16);
        out->sha     = b & (1u << 29);
        out->smep    = b & (1u << 7);
        out->smap    = b & (1u << 20);
    }

    if (max_ext >= 0x80000001u) {
        cpuid2(0x80000001u, 0, &a, &b, &c, &d);
        out->nx    = d & (1u << 20);
        out->amd_v = c & (1u << 2);
    }

    if (out->amd_v && max_ext >= 0x8000000Au) {
        cpuid2(0x8000000Au, 0, &a, &b, &c, &d);
        out->npt = d & 1u;
    }

    out->ept = out->vt_x;

    identify_microarch(out);
}
