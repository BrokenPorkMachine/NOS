#pragma once
#include <stdint.h>
#include "bootinfo.h"

#define PMM_BUDDY_MAX_ORDER 16 // up to 256 MiB per block (tune as needed)
void buddy_init(const bootinfo_t *bootinfo);
void *buddy_alloc(uint32_t order, int numa_node); // NUMA-aware
void buddy_free(void *addr, uint32_t order, int numa_node);
