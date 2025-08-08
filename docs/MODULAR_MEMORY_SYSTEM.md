# Modular Memory System Integration Guide

This document explains how to replace legacy memory management with the new modular system that combines a buddy allocator, NUMA awareness, high memory support, advanced paging, SMP safety, and a kernel heap.

## 1. Organize and Add Source Files

Add or update these components:

- `pmm_buddy.c` / `pmm_buddy.h` – core physical memory manager
- `paging_adv.c` / `paging_adv.h` – advanced paging
- `numa.c` / `numa.h` – NUMA region and affinity support
- `kheap.c` / `kheap.h` – optional kernel heap for dynamic allocations

Remove legacy bitmap-based `pmm.c` and related single-zone allocators.

## 2. Update the Build System

Add the new object files to the kernel build:

```make
OBJS += pmm_buddy.o paging_adv.o numa.o kheap.o
```

Ensure the corresponding headers are in the include path.

## 3. Initialize the Memory Subsystem

Update the kernel's early initialization to load NUMA information, the buddy allocator, and optional heap:

```c
#include "pmm_buddy.h"
#include "paging_adv.h"
#include "numa.h"
#include "kheap.h"

void kernel_main(const bootinfo_t *bootinfo) {
    numa_init(bootinfo);
    buddy_init(bootinfo);
    kheap_init();
}
```

## 4. Replace Physical Page Allocation Calls

Legacy calls:

```c
void *page = alloc_page();
free_page(page);
```

New API using the buddy allocator and NUMA awareness:

```c
int node = /* current CPU's NUMA node or 0 */;
void *page = buddy_alloc(0, node, 0);
buddy_free(page, 0, node);
```

Wrap these in macros if required for backward compatibility.

## 5. Use Advanced Paging

Replace old paging functions with the advanced interface:

```c
paging_map_adv(virt, phys, PAGE_PRESENT|PAGE_WRITABLE, order, node);
paging_unmap_adv(virt);
```

Page faults should be handled through:

```c
void paging_handle_fault(uint64_t err, uint64_t addr, int cpu_id);
```

## 6. SMP and NUMA-aware Allocation

Select the NUMA node for each allocation based on the running CPU or process affinity, e.g., via a helper:

```c
int current_cpu_node(void);
```

## 7. Advanced Features

- **Huge pages:**
  ```c
  paging_map_adv(virt, phys, PAGE_PRESENT|PAGE_WRITABLE|PAGE_HUGE_2MB, 9, node);
  ```
- **NUMA migration:**
  ```c
  buddy_migrate(page, old_node, new_node);
  ```
- **Kernel heap:** replace legacy `malloc`/`free` with `kalloc`/`kfree`.

## 8. Debugging and Validation

Call `buddy_debug_print()` during boot or runtime to inspect allocator state. Test with configurations that include high memory and multiple NUMA nodes.

## 9. Remove Legacy PMM Code

Delete old `alloc_page`, `free_page`, bitmap logic, and global variables from the previous physical memory manager.

## 10. Build and Test

Rebuild the kernel and verify functionality across QEMU, VM, or real hardware, exercising NUMA and high-memory scenarios.

