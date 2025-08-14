#include "vmm.h"
#include "pmm_buddy.h"
#include "numa.h"
#include "../../include/cpuid.h"
#include <string.h>

static inline uint64_t rdmsr(uint32_t msr) {
    uint32_t lo, hi;
    __asm__ volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return ((uint64_t)hi << 32) | lo;
}

static inline void wrmsr(uint32_t msr, uint64_t val) {
    uint32_t lo = (uint32_t)val;
    uint32_t hi = (uint32_t)(val >> 32);
    __asm__ volatile("wrmsr" :: "c"(msr), "a"(lo), "d"(hi));
}

void vmm_init(void) {
    uint32_t eax, ebx, ecx, edx;
    cpuid(0x80000001u, 0, &eax, &ebx, &ecx, &edx);
    if (edx & (1u << 20)) {
        uint64_t efer = rdmsr(0xC0000080);
        efer |= (1ULL << 11); // NXE
        wrmsr(0xC0000080, efer);
    }
    cpuid(7u, 0, &eax, &ebx, &ecx, &edx);
    uint64_t cr4;
    __asm__ volatile("mov %%cr4,%0" : "=r"(cr4));
    if (ebx & (1u << 7))
        cr4 |= (1ULL << 20); // SMEP
    if (ebx & (1u << 20))
        cr4 |= (1ULL << 21); // SMAP
    __asm__ volatile("mov %0,%%cr4" :: "r"(cr4));
}

static uint64_t *alloc_table(int node) {
    void *p = buddy_alloc(0, node, 0);
    if (!p) return NULL;
    memset(p, 0, PAGE_SIZE);
    return (uint64_t *)p;
}

static uint64_t *get_or_create(uint64_t *table, uint64_t index, uint64_t flags, int node) {
    if (!(table[index] & PAGE_PRESENT)) {
        uint64_t *new = alloc_table(node);
        if (!new) return NULL;
        table[index] = ((uint64_t)new) | flags | PAGE_PRESENT | PAGE_WRITABLE;
    }
    return (uint64_t *)(table[index] & ~0xFFFULL);
}

uint64_t *vmm_create_pml4(void) {
    int node = current_cpu_node();
    uint64_t *new_pml4 = alloc_table(node);
    if (!new_pml4) return NULL;
    uint64_t *kernel_pml4 = paging_kernel_pml4();
    /* Copy the full kernel PML4 so new threads inherit both lower and
       higher-half mappings.  The kernel currently runs in the lower
       half, so omitting those entries leaves new contexts without code
       or stack mappings, leading to early triple faults. */
    memcpy(new_pml4, kernel_pml4, 512 * sizeof(uint64_t));
    return new_pml4;
}

void vmm_map_page(uint64_t *pml4, uint64_t virt, uint64_t phys, uint64_t flags, uint32_t order, int node) {
    uint64_t pml4_i = (virt >> 39) & 0x1FF;
    uint64_t pdpt_i = (virt >> 30) & 0x1FF;
    uint64_t pd_i   = (virt >> 21) & 0x1FF;
    uint64_t pt_i   = (virt >> 12) & 0x1FF;

    uint64_t *pdpt_t = get_or_create(pml4, pml4_i, PAGE_USER, node);
    if (!pdpt_t) return;
    uint64_t *pd_t = get_or_create(pdpt_t, pdpt_i, PAGE_USER, node);
    if (!pd_t) return;

    if (order >= 9 || (flags & PAGE_SIZE_2MB)) {
        pd_t[pd_i] = (phys & ~0x1FFFFFULL) | (flags & ~PAGE_SIZE_2MB) |
                     PAGE_PRESENT | PAGE_WRITABLE | PAGE_SIZE_2MB;
        return;
    }

    uint64_t *pt_t = get_or_create(pd_t, pd_i, PAGE_USER, node);
    if (!pt_t) return;
    pt_t[pt_i] = (phys & ~0xFFFULL) | flags | PAGE_PRESENT;
}

void vmm_switch(uint64_t *pml4) {
    if (!pml4) return;
    __asm__ volatile("mov %0,%%cr3" :: "r"((uint64_t)pml4) : "memory");
}
