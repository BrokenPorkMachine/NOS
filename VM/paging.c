#include "paging.h"
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
