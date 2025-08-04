#ifndef NOS_CPUID_H
#define NOS_CPUID_H
#include <stdint.h>

static inline void cpuid(uint32_t eax_in, uint32_t ecx_in,
                         uint32_t *eax, uint32_t *ebx,
                         uint32_t *ecx_out, uint32_t *edx)
{
    __asm__ volatile("cpuid"
                     : "=a"(*eax), "=b"(*ebx), "=c"(*ecx_out), "=d"(*edx)
                     : "a"(eax_in), "c"(ecx_in));
}

static inline uint32_t cpuid_logical_cpu_count(void)
{
    uint32_t max_leaf, eax, ebx, ecx, edx;
    cpuid(0, 0, &max_leaf, &ebx, &ecx, &edx);
    if (max_leaf >= 0x0B) {
        cpuid(0x0B, 0, &eax, &ebx, &ecx, &edx);
        uint32_t count = ebx & 0xffffu;
        return count ? count : 1u;
    }
    if (max_leaf >= 1) {
        cpuid(1, 0, &eax, &ebx, &ecx, &edx);
        uint32_t count = (ebx >> 16) & 0xffu;
        return count ? count : 1u;
    }
    return 1u;
}

static inline uint32_t cpuid_bsp_apic_id(void)
{
    uint32_t eax, ebx, ecx, edx;
    cpuid(1, 0, &eax, &ebx, &ecx, &edx);
    return (ebx >> 24) & 0xffu;
}

#endif // NOS_CPUID_H
