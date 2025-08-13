// kernel/loader_vm_pmm_shims.c
// Adapts your existing VM/PMM APIs to the names expected by elf_paged_loader.c

#include <stddef.h>
#include <stdint.h>
#include "VM/paging_adv.h"
#include "VM/pmm_buddy.h"
#include "VM/numa.h"
#include "VM/legacy_heap.h"

// Reserve a contiguous VA range (simple heap-backed implementation)
void* vmm_reserve(size_t size, size_t align) {
    (void)align; // legacy_kmalloc provides page alignment
    return legacy_kmalloc(size);
}

// Map one 4K page at VA->PA with protection flags
// Protection flags
#define PROT_EXEC  0x1
#define PROT_WRITE 0x2
#define PROT_READ  0x4
#define PROT_USER  0x8

static inline uint64_t prot_to_flags(int prot) {
    uint64_t flags = 0;
    if (prot & PROT_WRITE) flags |= PAGE_WRITABLE;
    if (prot & PROT_USER)  flags |= PAGE_USER;
    if (!(prot & PROT_EXEC)) flags |= PAGE_NO_EXEC;
    return flags;
}

int vmm_map(void* va, uintptr_t pa, int prot) {
    paging_map_adv((uint64_t)va, pa, prot_to_flags(prot), 0, current_cpu_node());
    return 0;
}

// Change page protections on a VA range
void vmm_prot(void* va, size_t size, int prot) {
    uint64_t flags = prot_to_flags(prot);
    for (size_t off = 0; off < size; off += PAGE_SIZE) {
        uint64_t phys = paging_virt_to_phys_adv((uint64_t)va + off);
        if (phys)
            paging_map_adv((uint64_t)va + off, phys, flags, 0, current_cpu_node());
    }
}

// Unmap a VA range
void vmm_unmap(void* va, size_t size) {
    for (size_t off = 0; off < size; off += PAGE_SIZE)
        paging_unmap_adv((uint64_t)va + off);
}

// Check that VA is mapped with execute permission (best-effort)
int vmm_is_mapped_x(void* va) {
    return paging_virt_to_phys_adv((uint64_t)va) != 0;
}

// Physical page allocator (4K pages)
uintptr_t pmm_alloc_page(void) {
    void* p = buddy_alloc(0, current_cpu_node(), 0);
    return (uintptr_t)p;
}
