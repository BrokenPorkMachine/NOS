#include "paging.h"
#include "pmm.h"
#include "../src/libc.h"
#include <stdint.h>

// Paging flags (should match paging.h)
#define PAGE_PRESENT 0x1
#define PAGE_RW      0x2
#define PAGE_SIZE    4096ULL
#define PAGE_PS      (1ULL << 7)  // Page Size (for 2MB pages)

// Static page tables (identity map 0–2MB)
static uint64_t __attribute__((aligned(PAGE_SIZE))) pml4[512];
static uint64_t __attribute__((aligned(PAGE_SIZE))) pdpt[512];
static uint64_t __attribute__((aligned(PAGE_SIZE))) pd[512];

void paging_init(void) {
    // Zero out the tables for safety
    for (int i = 0; i < 512; ++i) {
        pml4[i] = pdpt[i] = pd[i] = 0;
    }

    // Map 2MB at virtual address 0 (identity map)
    pd[0] = PAGE_PRESENT | PAGE_RW | PAGE_PS;           // 2MB page at 0x0
    pdpt[0] = ((uint64_t)pd) | PAGE_PRESENT | PAGE_RW;  // PD pointer
    pml4[0] = ((uint64_t)pdpt) | PAGE_PRESENT | PAGE_RW; // PDPT pointer

    // Load page tables (CR3)
    asm volatile("mov %0, %%cr3" : : "r"(pml4));

    // Enable PAE (Physical Address Extension), SMEP, SMAP in CR4
    uint64_t cr4;
    asm volatile("mov %%cr4, %0" : "=r"(cr4));
    cr4 |= (1ULL << 5);   // PAE
    cr4 |= (1ULL << 20);  // SMEP (optional, safe on modern CPUs)
    cr4 |= (1ULL << 21);  // SMAP (optional, safe on modern CPUs)
    asm volatile("mov %0, %%cr4" : : "r"(cr4));

    // Enable Long Mode (LME) and NXE (No-Execute) in EFER MSR
    uint64_t efer;
    uint32_t edx = 0;
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
        table[index] = ((uint64_t)new) | flags | PAGE_PRESENT | PAGE_RW;
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
                     PAGE_PRESENT | PAGE_RW;
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

    if (pd_t[pd_i] & PAGE_PS) {
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

    if (pd_t[pd_i] & PAGE_PS)
        return (pd_t[pd_i] & ~0x1FFFFFULL) | (virt & 0x1FFFFFULL);

    uint64_t *pt_t = (uint64_t *)(pd_t[pd_i] & ~0xFFFULL);
    if (!(pt_t[pt_i] & PAGE_PRESENT)) return 0;
    return (pt_t[pt_i] & ~0xFFFULL) | (virt & 0xFFFULL);
}

uint64_t paging_get_pml4(void) {
    return (uint64_t)pml4;
}
