// kernel/loader_vm_pmm_shims.c
// Adapts your existing VM/PMM APIs to the names expected by elf_paged_loader.c

#include <stddef.h>
#include <stdint.h>

// ====== EDIT THESE externs to match your kernel ======
// Examples shown — replace with your real functions from paging_adv.c / pmm_buddy.c

// Reserve a contiguous VA range (no pages mapped yet)
extern void* paging_reserve_range(size_t size, size_t align);
// Map one 4K page at VA->PA with protection flags
extern int   paging_map_page(void* va, uintptr_t pa, int prot);
// Change page protections on a VA range
extern void  paging_set_prot(void* va, size_t size, int prot);
// Unmap a VA range
extern void  paging_unmap_range(void* va, size_t size);
// Check that VA is mapped with execute permission
extern int   paging_is_executable(void* va);

// Physical page allocator (should already exist in pmm_buddy.c)
extern uintptr_t pmm_alloc_page_4k(void);

// ====== Adapters exported under the names the loader expects ======
void* vmm_reserve(size_t size, size_t align) { return paging_reserve_range(size, align); }
int   vmm_map(void* va, uintptr_t pa, int prot) { return paging_map_page(va, pa, prot); }
void  vmm_prot(void* va, size_t size, int prot) { paging_set_prot(va, size, prot); }
void  vmm_unmap(void* va, size_t size) { paging_unmap_range(va, size); }
int   vmm_is_mapped_x(void* va) { return paging_is_executable(va); }

// Unify allocator name
uintptr_t pmm_alloc_page(void) { return pmm_alloc_page_4k(); }
