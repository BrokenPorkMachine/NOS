#pragma once
#include <stdint.h>

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

#ifdef __cplusplus
}
#endif
