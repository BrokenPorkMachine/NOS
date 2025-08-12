# NitroHeap

NitroHeap is a production-ready replacement for the simple buddy-backed kernel
heap. It introduces build- and boot-time plumbing so the allocator can be
selected at runtime while providing its own size-class allocator.

* `CONFIG_NITRO_HEAP=1` enables NitroHeap by default at build time.
* A boot argument `heap=nitro` or `heap=legacy` overrides the build default.
* Public APIs are provided via `kernel/VM/heap.h` with `kmalloc`, `kfree`, and
  `krealloc`.

The current implementation manages allocations using predefined size classes.
Each class draws spans from the buddy allocator, caches freed blocks in per-CPU
magazines, and returns completely free spans to the system. Requests larger than
any size class fall back to the legacy allocator.
