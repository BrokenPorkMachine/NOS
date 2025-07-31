#include "cpu.h"

static inline void cpuid(uint32_t eax_in, uint32_t ecx_in,
                         uint32_t *eax, uint32_t *ebx,
                         uint32_t *ecx, uint32_t *edx)
{
    __asm__ volatile("cpuid"
                     : "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx)
                     : "a"(eax_in), "c"(ecx_in));
}

uint32_t cpu_detect_logical_count(void)
{
    uint32_t max_leaf, ebx, ecx, edx, eax;
    cpuid(0, 0, &max_leaf, &ebx, &ecx, &edx);
    if (max_leaf >= 0x0B) {
        uint32_t count = 0;
        cpuid(0x0B, 0, &eax, &ebx, &ecx, &edx);
        if (ebx != 0)
            count = ebx & 0xffff;
        if (count == 0) count = 1;
        return count;
    }
    if (max_leaf >= 1) {
        cpuid(1, 0, &eax, &ebx, &ecx, &edx);
        uint32_t count = (ebx >> 16) & 0xff;
        if (count == 0) count = 1;
        return count;
    }
    return 1;
}
