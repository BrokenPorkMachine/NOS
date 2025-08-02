#pragma once
#include <stdint.h>

// Paging flags for x86_64
#define PAGE_PRESENT    0x001
#define PAGE_WRITABLE   0x002
#define PAGE_USER       0x004
#define PAGE_WRITE_THROUGH 0x008
#define PAGE_CACHE_DISABLE 0x010
#define PAGE_SIZE_2MB   0x080
#define PAGE_NO_EXEC    (1ULL << 63)

#define PAGE_SIZE       0x1000ULL  // 4 KiB

#ifdef __cplusplus
extern "C" {
#endif

// Core initialization: set up kernel page tables, enable paging, map kernel
void paging_init(void);

// Map a single page (virtual -> physical) with flags
void paging_map(uint64_t virt, uint64_t phys, uint64_t flags);

// Unmap a single page (virtual)
void paging_unmap(uint64_t virt);

// Translate virtual address to physical, return 0 if not mapped
uint64_t paging_virt_to_phys(uint64_t virt);

// Flush the TLB for a specific virtual address (after mapping/unmapping)
static inline void paging_flush_tlb(uint64_t virt) {
    asm volatile("invlpg (%0)" :: "r"(virt) : "memory");
}

// Optionally: expose PML4 base for manual manipulation/debugging
uint64_t paging_get_pml4(void);

#ifdef __cplusplus
}
#endif
