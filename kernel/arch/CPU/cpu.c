#include "cpu.h"
#include "../../../include/cpuid.h"

uint32_t cpu_detect_logical_count(void)
{
    return cpuid_logical_cpu_count();
}
