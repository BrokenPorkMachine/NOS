#include "paging.h"
#include <stdint.h>

#define PAGE_PRESENT 0x1
#define PAGE_RW      0x2
#define PAGE_SIZE    4096ULL

// Allocate page tables statically for now
static uint64_t __attribute__((aligned(PAGE_SIZE))) pml4[512];
static uint64_t __attribute__((aligned(PAGE_SIZE))) pdpt[512];
static uint64_t __attribute__((aligned(PAGE_SIZE))) pd[512];

void paging_init(void) {
    // Identity map 0..2MB as RW (for kernel, stack, VGA, etc.)

    // Set PD to 2MB page, present+RW
    pd[0] = 0 | PAGE_PRESENT | PAGE_RW | (1 << 7); // 2MB page (PS bit = 1)
    pdpt[0] = (uint64_t)pd | PAGE_PRESENT | PAGE_RW;
    pml4[0] = (uint64_t)pdpt | PAGE_PRESENT | PAGE_RW;

    // Load page table base (CR3)
    asm volatile("mov %0, %%cr3" : : "r"(pml4));

    // Enable paging and write-protect kernel
    uint64_t cr0, cr4, efer;
    asm volatile("mov %%cr4, %0" : "=r"(cr4));
    cr4 |= (1 << 5); // PAE
    asm volatile("mov %0, %%cr4" : : "r"(cr4));

    asm volatile("mov $0xC0000080, %%ecx; rdmsr" : "=a"(efer) : : "ecx", "edx");
    efer |= (1 << 8); // LME (Long mode enable)
    asm volatile("mov $0xC0000080, %%ecx; wrmsr" : : "a"(efer), "d"(0) : "ecx");

    asm volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= (1 << 31); // PG (paging enable)
    cr0 |= (1 << 16); // WP (write protect kernel)
    asm volatile("mov %0, %%cr0" : : "r"(cr0));
}
