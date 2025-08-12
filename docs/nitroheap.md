# NitroHeap

NitroHeap is a planned replacement for the simple buddy-backed kernel heap.  This
initial skeleton introduces the build- and boot-time plumbing so the allocator
can be selected at runtime.

* `CONFIG_NITRO_HEAP=1` enables NitroHeap by default at build time.
* A boot argument `heap=nitro` or `heap=legacy` overrides the build default.
* Public APIs are provided via `kernel/VM/heap.h` with `kmalloc`, `kfree`, and
  `krealloc`.

The current implementation delegates to the legacy buddy allocator while the
full NitroHeap with size classes, magazines, and span management is developed.
