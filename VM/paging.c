#include "paging.h"
#include "pmm.h"
#include "../src/libc.h"
#include <stdint.h>

static inline void cpuid(uint32_t eax_in, uint32_t ecx_in,
                         uint32_t *eax, uint32_t *ebx,
                         uint32_t *ecx_out, uint32_t *edx) {
    __asm__ volatile("cpuid"
                     : "=a"(*eax), "=b"(*ebx), "=c"(*ecx_out), "=d"(*edx)
                     : "a"(eax_in), "c"(ecx_in));
}

// Static page tables (identity map first 4GB)
static uint64_t __attribute__((aligned(PAGE_SIZE))) pml4[512];
static uint64_t __attribute__((aligned(PAGE_SIZE))) pdpt[512];
static uint64_t __attribute__((aligned(PAGE_SIZE))) pd[4][512];

void paging_init(void) {
    // Zero out the tables for safety
    for (int i = 0; i < 512; ++i) {
        pml4[i] = pdpt[i] = 0;
    }
    for (int j = 0; j < 4; ++j)
        for (int i = 0; i < 512; ++i)
            pd[j][i] = 0;

    // Identity map the first 4GB using 2MB pages
    for (int j = 0; j < 4; ++j) {
        for (int i = 0; i < 512; ++i) {
            uint64_t addr = ((uint64_t)j << 30) + ((uint64_t)i << 21);
            pd[j][i] = addr | PAGE_PRESENT | PAGE_WRITABLE | PAGE_SIZE_2MB;
        }
        pdpt[j] = ((uint64_t)pd[j]) | PAGE_PRESENT | PAGE_WRITABLE;
    }

    pml4[0] = ((uint64_t)pdpt) | PAGE_PRESENT | PAGE_WRITABLE;

    // Load page tables (CR3)
    asm volatile("mov %0, %%cr3" : : "r"(pml4));

    // Enable PAE and optional protection features in CR4
    uint64_t cr4;
    asm volatile("mov %%cr4, %0" : "=r"(cr4));
    cr4 |= (1ULL << 5);   // PAE

    uint32_t eax, ebx, ecx, edx;
    cpuid(7, 0, &eax, &ebx, &ecx, &edx);
    if (ebx & (1u << 7))  // SMEP supported
        cr4 |= (1ULL << 20);
    if (ebx & (1u << 20)) // SMAP supported
        cr4 |= (1ULL << 21);

    asm volatile("mov %0, %%cr4" : : "r"(cr4));

    // Enable Long Mode (LME) and NXE (No-Execute) in EFER MSR
    uint64_t efer;
    edx = 0;
    asm volatile(
        "mov $0xC0000080, %%ecx; rdmsr"
        : "=a"(efer), "=d"(edx) :: "ecx"
    );
    efer |= (1ULL << 8);   // LME (Long Mode Enable)
    efer |= (1ULL << 11);  // NXE (No-Execute Enable)
    asm volatile(
        "mov $0xC0000080, %%ecx; wrmsr"
        :: "a"(efer), "d"(edx) : "ecx"
    );

    // Enable Paging and Write-Protect in CR0
    uint64_t cr0;
    asm volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= (1ULL << 31); // PG (Paging Enable)
    cr0 |= (1ULL << 16); // WP (Write Protect: prevents kernel from writing to RO pages)
    asm volatile("mov %0, %%cr0" : : "r"(cr0));
}

static uint64_t *alloc_table(void) {
    void *page = alloc_page();
    if (!page)
        return NULL;
    memset(page, 0, PAGE_SIZE);
    return (uint64_t *)page;
}

static uint64_t *get_or_create(uint64_t *table, uint64_t index, uint64_t flags) {
    if (!(table[index] & PAGE_PRESENT)) {
        uint64_t *new = alloc_table();
        if (!new) return NULL;
        table[index] = ((uint64_t)new) | flags | PAGE_PRESENT | PAGE_WRITABLE;
    }
    return (uint64_t *)(table[index] & ~0xFFFULL);
}

void paging_map(uint64_t virt, uint64_t phys, uint64_t flags) {
    uint64_t pml4_i = (virt >> 39) & 0x1FF;
    uint64_t pdpt_i = (virt >> 30) & 0x1FF;
    uint64_t pd_i   = (virt >> 21) & 0x1FF;
    uint64_t pt_i   = (virt >> 12) & 0x1FF;

    uint64_t *pdpt_t = get_or_create(pml4, pml4_i, PAGE_USER);
    if (!pdpt_t) return;
    uint64_t *pd_t = get_or_create(pdpt_t, pdpt_i, PAGE_USER);
    if (!pd_t) return;

    if (flags & PAGE_SIZE_2MB) {
        pd_t[pd_i] = (phys & ~0x1FFFFFULL) | (flags & ~PAGE_SIZE_2MB) |
                     PAGE_PRESENT | PAGE_WRITABLE;
        paging_flush_tlb(virt);
        return;
    }

    uint64_t *pt_t = get_or_create(pd_t, pd_i, PAGE_USER);
    if (!pt_t) return;
    pt_t[pt_i] = (phys & ~0xFFFULL) | flags | PAGE_PRESENT;
    paging_flush_tlb(virt);
}

void paging_unmap(uint64_t virt) {
    uint64_t pml4_i = (virt >> 39) & 0x1FF;
    uint64_t pdpt_i = (virt >> 30) & 0x1FF;
    uint64_t pd_i   = (virt >> 21) & 0x1FF;
    uint64_t pt_i   = (virt >> 12) & 0x1FF;

    if (!(pml4[pml4_i] & PAGE_PRESENT)) return;
    uint64_t *pdpt_t = (uint64_t *)(pml4[pml4_i] & ~0xFFFULL);
    if (!(pdpt_t[pdpt_i] & PAGE_PRESENT)) return;
    uint64_t *pd_t = (uint64_t *)(pdpt_t[pdpt_i] & ~0xFFFULL);
    if (!(pd_t[pd_i] & PAGE_PRESENT)) return;

    if (pd_t[pd_i] & PAGE_SIZE_2MB) {
        pd_t[pd_i] = 0;
        paging_flush_tlb(virt);
        return;
    }

    uint64_t *pt_t = (uint64_t *)(pd_t[pd_i] & ~0xFFFULL);
    pt_t[pt_i] = 0;
    paging_flush_tlb(virt);
}

uint64_t paging_virt_to_phys(uint64_t virt) {
    uint64_t pml4_i = (virt >> 39) & 0x1FF;
    uint64_t pdpt_i = (virt >> 30) & 0x1FF;
    uint64_t pd_i   = (virt >> 21) & 0x1FF;
    uint64_t pt_i   = (virt >> 12) & 0x1FF;

    if (!(pml4[pml4_i] & PAGE_PRESENT)) return 0;
    uint64_t *pdpt_t = (uint64_t *)(pml4[pml4_i] & ~0xFFFULL);
    if (!(pdpt_t[pdpt_i] & PAGE_PRESENT)) return 0;
    uint64_t *pd_t = (uint64_t *)(pdpt_t[pdpt_i] & ~0xFFFULL);
    if (!(pd_t[pd_i] & PAGE_PRESENT)) return 0;

    if (pd_t[pd_i] & PAGE_SIZE_2MB)
        return (pd_t[pd_i] & ~0x1FFFFFULL) | (virt & 0x1FFFFFULL);

    uint64_t *pt_t = (uint64_t *)(pd_t[pd_i] & ~0xFFFULL);
    if (!(pt_t[pt_i] & PAGE_PRESENT)) return 0;
    return (pt_t[pt_i] & ~0xFFFULL) | (virt & 0xFFFULL);
}

uint64_t paging_get_pml4(void) {
    return (uint64_t)pml4;
}
