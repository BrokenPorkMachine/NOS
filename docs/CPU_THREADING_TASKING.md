# CPU Handling, Caches, Threading and Tasking

This document details how NitrOS manages processor resources, cache coherence
and the execution of tasks and threads.

## CPU Handling

- CPUs are discovered via ACPI tables and `CPUID` feature probes.
- The bootstrap processor initializes paging and brings up application
  processors using the local APIC.
- Each core owns a per-CPU structure with scheduler state, current task pointer
  and statistics counters.
- Future work includes NUMA-aware core groups and topology hints for agents.

## Cache Strategy

- N2 assumes hardware cache coherence but uses memory barriers around MMIO and
  shared structures to prevent reordering.
- Page table changes trigger TLB shootdowns via inter-processor interrupts so
  all cores observe new mappings.
- Planned optimizations include cache coloring and use of non-temporal moves for
  bulk transfers.

## Threading & Tasking

- Tasks provide isolated address spaces while threads represent schedulable
  contexts within a task.
- Each CPU maintains a run queue; the scheduler performs round-robin
  time slicing and context switches by updating CR3 and saving registers.
- Threads may communicate through IPC mail boxes and shared memory regions.
- An idle thread per core executes `hlt` when no runnable work exists.
- Priority and affinity scheduling policies are on the roadmap.

