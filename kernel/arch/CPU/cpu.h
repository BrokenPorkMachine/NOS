#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Detect the total number of logical processors (hardware threads) in the system.
 *
 * This queries CPUID topology leaves (0x1F/0x0B) when available, falling back to
 * legacy CPUID.1 EBX[23:16], and always returns at least 1.
 *
 * The result is cached after the first call for fast subsequent lookups.
 *
 * @return Logical processor count (>=1).
 */
uint32_t cpu_detect_logical_count(void);

struct cpu_features {
    char vendor[13];
    char brand[49];
    char microarch[32];

    uint32_t family;
    uint32_t model;
    uint32_t stepping;

    /* Instruction Set Extensions */
    bool mmx;
    bool sse;
    bool sse2;
    bool sse3;
    bool ssse3;
    bool sse41;
    bool sse42;
    bool avx;
    bool avx2;
    bool avx512f;
    bool fma;
    bool bmi1;
    bool bmi2;
    bool sha;

    /* Virtualization Features */
    bool vt_x;
    bool amd_v;
    bool ept;
    bool npt;

    /* Security Features */
    bool smep;
    bool smap;
    bool nx;
};

void cpu_detect_features(struct cpu_features *out);

#ifdef __cplusplus
}
#endif
