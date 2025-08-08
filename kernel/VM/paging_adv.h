#pragma once
#include <stdint.h>
#include "pmm_buddy.h"

#ifdef __cplusplus
extern "C" {
#endif

// Extended paging flags (combine with existing flags)
#define PAGE_HUGE_2MB 0x080
#define PAGE_HUGE_1GB 0x200

void paging_map_adv(uint64_t virt, uint64_t phys, uint64_t flags, uint32_t order, int numa_node);
void paging_unmap_adv(uint64_t virt);
uint64_t paging_virt_to_phys_adv(uint64_t virt);

void paging_handle_fault(uint64_t err, uint64_t addr, int cpu_id);

#ifdef __cplusplus
}
#endif

