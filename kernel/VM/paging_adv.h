#pragma once
#include <stdint.h>
#include "pmm_buddy.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef PAGE_SIZE
#define PAGE_SIZE 4096ULL
#endif

// Paging flags for x86_64
#define PAGE_PRESENT        0x001ULL
#define PAGE_WRITABLE       0x002ULL
#define PAGE_USER           0x004ULL
#define PAGE_WRITE_THROUGH  0x008ULL
#define PAGE_CACHE_DISABLE  0x010ULL
#define PAGE_NO_EXEC        (1ULL << 63)

// Extended paging flags
#define PAGE_HUGE_2MB 0x080ULL
#define PAGE_HUGE_1GB 0x200ULL
#define PAGE_SIZE_2MB  PAGE_HUGE_2MB

// Map a virtual address to a physical one on a preferred NUMA node.
void paging_map_adv(uint64_t virt, uint64_t phys, uint64_t flags, uint32_t order, int numa_node);
void paging_unmap_adv(uint64_t virt);
uint64_t paging_virt_to_phys_adv(uint64_t virt);

void paging_handle_fault(uint64_t err, uint64_t addr, int cpu_id);

/* Debug helper: retrieve mapping info for a virtual address.
 * Returns 1 if mapped and fills phys and flags (raw PTE/PDE).
 * Returns 0 if unmapped. */
int paging_lookup_adv(uint64_t virt, uint64_t *phys, uint64_t *flags);

static inline void paging_flush_tlb(uint64_t virt) {
    asm volatile("invlpg (%0)" :: "r"(virt) : "memory");
}

#ifdef __cplusplus
}
#endif

