# NitrOS Memory Management Design

This document outlines a proposed secure and optimized memory mapping and management system for the NitrOS microkernel.

## Goals

- Provide isolated address spaces for each user task/server
- Enable fine-grained control over permissions (RWX) and caching
- Support paging with dynamic allocation and reclaiming of physical memory
- Harden the kernel and user space against memory corruption exploits

## Key Components

1. **Physical Memory Manager (PMM)**
   - Initializes from the UEFI memory map delivered via `bootinfo_t`
   - Maintains a bitmap or buddy allocator of available physical frames
   - Provides `alloc_page()` / `free_page()` primitives for the kernel
   - Marks kernel code/data as read-only after initialization

2. **Virtual Memory Manager (VMM)**
   - Builds per-task page tables (PML4 -> PDP -> PD -> PT)
   - Supports 4 KiB and 2 MiB pages to balance granularity and TLB usage
   - Detects CPU features via `CPUID` and enables NX, SMEP and SMAP for user mappings
   - Kernel memory mapped high in address space and shared read-only with tasks
   - Maps user code/data stacks with appropriate permissions and ASLR offsets

3. **System Calls for Mapping**
   - `sys_map(addr, phys, flags)` – map physical frame with flags into task space
   - `sys_unmap(addr)` – remove mapping and flush TLB
   - `sys_brk` / `sys_mmap` style calls to grow heap or map files from servers

4. **Security Features**
   - Randomize task base addresses on creation (ASLR)
   - Apply guard pages around stacks and critical regions
   - Zero on free (both physical and virtual) to avoid information leaks
   - Validate all user-supplied addresses in syscalls

5. **Optimization Techniques**
   - Use large pages (2 MiB) for kernel text/data and for contiguous memory
   - Lazy allocation of page tables (allocate lower levels on demand)
   - Reference counting shared frames to minimize copy
   - Batch TLB flushes where possible (e.g., per-task CR3 switch)

6. **Cache Management**
   - Separate per-CPU page caches reduce contention and take advantage of
     cache locality
   - TLB shootdowns are coordinated via IPIs when mappings change
   - Planned support for cache coloring and non-temporal copies for IO
     heavy workloads

7. **Recent Additions**
   - Copy-on-write tracking with a simple page fault handler
   - Experimental NUMA region discovery from the boot memory map
   - IPC shared memory buffers protected by rights masks
   - Human-readable boot memory map logging for easier debugging
   - Refactored MMIO helpers with explicit memory barriers
   - Shared memory creation now enforces page alignment and exposes rights checks

## Virtual Address Layout

- `0x0000000000000000` – `0x00007FFFFFFFFFFF`: per-task user space with randomized bases, guarded stacks and dedicated heap/IPC zones
- `0xFFFF800000000000` – `0xFFFF8FFFFFFFFFFF`: kernel text and static data mapped via 2 MiB pages
- `0xFFFF900000000000` – `0xFFFF9FFFFFFFFFFF`: NOSM modules, kept read-only to other tasks
- `0xFFFFC00000000000` – `0xFFFFFFFFFFFFFFFF`: MMIO and device apertures isolated from regular memory

## Boot Sequence Overview

1. Bootloader collects the UEFI memory map and populates `bootinfo_t`
2. Kernel PMM processes the map, reserving regions for kernel and boot modules
3. VMM creates an initial kernel address space and enables paging in long mode
4. When tasks are created, new page tables are cloned from a template and
   customized per-task
5. Scheduler switches tasks by loading the task's CR3, automatically flushing
   relevant TLB entries

---

This design aims to balance security (isolated address spaces, strict page
permissions) with performance (large pages, lazy allocation) while fitting the
microkernel architecture described in `AGENTS.md`.
