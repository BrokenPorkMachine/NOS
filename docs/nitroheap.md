# NitroHeap temporary heap

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
any size class allocate dedicated buddy spans and free them directly back to the
buddy manager.

# FINAL NitroHeap: architecture at a glance

### 1) Partitioned, typed heaps (no cross-type reuse)
- **Idea:** Adopt Chrome’s *partitioning* principle: every object “class” (by size + trait) lives in its own partition so freed memory from one class can’t satisfy another (strong UAF/Type-Confusion mitigation), with page-granular guard rails. PartitionAlloc proved this model lowers fragmentation and raises security.
- **What’s new:** NitrOS makes partitions **first-class kernel objects** with cgroup-like controls (quotas, telemetry, pressure signals) and explicit *Traits* (see §9).

### 2) Per-CPU heaps with NUMA-aware magazines
- **Idea:** Each CPU has a *local arena* and ring buffers (“magazines”) for hot size classes; cross-CPU frees are a single atomic push to a remote-free list (inspired by mimalloc) to avoid locks on the fast path.
- **What’s new:**  
  - **NUMA policy** per partition: local-only, interleave, or “hot-follow” (migrates magazines to the CPU that actually touches the objects most).  
  - **Congestion contracts:** if a magazine’s contention exceeds a threshold, NitroHeap transparently re-shards that size class for that core only (no global stop-the-world).

### 3) Slab + bump “fusion” with fixed inner sizes
- **Small/medium allocations (≤128 KiB)**: slabbed in *exact* size classes (power-of-two + a few Fibonacci gaps to reduce internal frag).  
- **Tiny (≤512 B)**: per-CPU bump blocks inside guard-slabs to hit L1 and minimize pointer chasing.  
- **Large (>~128 KiB)**: directly mapped via VM extents with alignment to huge pages when available (like jemalloc’s extent model).

### 4) Metadata out-of-line + encoded free lists
- **Idea:** Keep all allocator metadata in a *shadow region* (not adjacent to user data) and encode freelist pointers with per-partition keys (hardened_malloc style). This blocks classic heap-metadata corruption.
- **What’s new:** Metadata pages are **execute-never, read-mostly**, and randomly offset per partition; optional *PAC/BTI/MTE* integration when the CPU supports it (ARMv8.5+).

### 5) Temporal-safety by default: epoch quarantine + color cycling
- **Quarantine on free:** freed chunks wait at least one *reuse epoch* before reentering fast lists. Epochs are per-CPU and staggered (low tail latency, high UAF resistance).  
- **Color cycling:** each reallocation flips a small random *color* value stored in the shadow metadata, mixed into pointer encodings and cookie checks.  
- **Optional strong mode:** put freed objects into mini-pools surrounded by guard pages (sampled), à la GWP-ASan, but *always on* at low rate globally.

### 6) Guarded sampling everywhere (prod-safe)
- NitroHeap continuously *samples* (configurable 1 in N allocations) into a **Guarded Pool** with: full redzones, immediate trap on OOB/UAF, and exact stack traces. This borrows the proven production model of GWP-ASan and brings it inside the OS allocator with negligible global overhead.

### 7) Fragmentation control with proactive defragment lanes
- **Partition compaction:** For slabbed classes, background threads evacuate sparsely used slabs into denser ones using **movable allocations** *where marked movable* (see “traits”). Non-movable allocations are never relocated.  
- **Extent reclaming:** Large extents are split/merged like jemalloc; idle pages are MADV_DONTNEED’d quickly, and huge pages are promoted/demoted opportunistically.  
- Compared to Windows Segment Heap (which emphasizes footprint & concurrency), NitroHeap adds **OS-level relocation contracts** for explicitly movable allocations—unlocking fragmentation wins without hurting legacy code.

### 8) Deterministic O(1) fast paths (no locks on hot path)
- **Alloc:** per-CPU magazine pop → if empty, take from local slab; if empty, refill from partition’s global pool with a single CAS ticket.  
- **Free:** thread-local → push to local list; cross-CPU free → push to remote-free list (single CAS), later *harvested* by the owning CPU (mimalloc-like).

### 9) **Traits API** (the killer feature)
Callers can (optionally) tag allocations with **traits** that map to partitions and policies, without new syscalls for most code (C library exposes `mallocx(flags)`; OS runtimes expose attributes):

- `SECURE_STRICT` → heavier hardening: zero-on-alloc+free, shorter reuse epochs, higher guard sampling.  
- `LOW_LATENCY` → bigger magazines, larger bump blocks, relaxed quarantine.  
- `MOVABLE` → allocator may relocate; returns a *stable handle* (like a small handle table ID), with zero-copy growth/shrink and background compaction.  
- `SHARED_RW` → tuned for cross-thread frees and cache-line placement.  
- `PERSISTENT` → NUMA-interleaved, huge-page preferred, slow-path optimized for long-lived objects.  
- `TELEMETRY_SILENT` → opt out of per-allocation stack sampling.

This is how NitroHeap beats “one-size-fits-all” allocators in both perf and memory shape, *without* forcing app-wide switches.

### 10) First-class observability
- **Always-on, low-overhead counters** per partition/CPU: alloc/free rate, contention, slab fullness, quarantine backlog, remote-free backlog.  
- **Flight recorder** (ring buffer) for sampled allocations: size, site, lifetime, partition, CPU—exported via ETW-like tracepoints for live debugging.  
- **Heap Contracts**: an admin can enforce “App X must keep Partition Y under 600 MiB; if exceeded, raise memory pressure signal and enlarge quarantine by 2×”.

### 11) Security hardening (stack of defenses)
- Out-of-line metadata + pointer encoding (per-boot and per-partition keys).  
- Per-allocation cookies tied to partition secret + color.  
- Guarded sampling pool (GWP-ASan-style).  
- Deferred reuse (epoch quarantine) as a systemic UAF mitigator.  
- Optional MTE (ARM) with *per-partition tag palettes* (minimizes tag reuse).  
- Randomized slab start offsets and guard-slab interleaving for tiny classes.  
- **No in-band metadata** in user-writable pages.

### 12) Compatibility modes
- **glibc/ptmalloc shims**, **jemalloc-style APIs**, and **Windows-like heap APIs**—runtimes can map to NitroHeap traits under the hood so existing software benefits without source changes.

---

## Why this should outperform the status quo

- **Latency:** per-CPU magazines + fusion slabs/bump blocks keep the *alloc/free* path to a handful of predictable instructions—like mimalloc’s best paths—while remote frees are still O(1) CAS.  
- **Fragmentation & footprint:** strict partitioning and movable-trait compaction compare favorably to Segment Heap’s footprint gains, and jemalloc’s extent management, but add *object-level* compaction where allowed.  
- **Scalability:** NUMA-aware magazines and congestion-driven re-sharding avoid global contention that hurts ptmalloc and some jemalloc configs under extreme thread counts.  
- **Security:** it combines hardened_malloc’s metadata isolation/encoding with always-on *production-safe* guard sampling (GWP-ASan), reducing the exploitability of UAF/overflow bugs—and *finding them* in the wild.  
- **Observability:** built-in flight recorder + contracts simplify memory SLOs and debugging—an area where many allocators rely on ad-hoc tooling.

---

## Practical implementation notes (so you can build it)

- **Kernel interface:**  
  - `sys_heapctl(op, args)` to create/destroy partitions, set quotas/traits, query stats.  
  - VM integration for extent mapping, huge page promotion, MTE tag domains.  
- **Libc surface:**  
  - `malloc`, `free` → default partition with policy auto-tuned by process class.  
  - `mallocx(size, flags)` / `rallocx` / `dallocx` (jemalloc-like) to pass traits without breaking ABI.  
  - `halloc(size, traitset)` → returns **handle** for `MOVABLE` allocations; `hptr(handle)` grants a temporary direct pointer with epoch validity.  
- **Background services:** low-priority *Defragger* and *Harvester* per NUMA node (compaction, reclaim, remote-free harvest).  
- **Safe defaults:** processes start in `BALANCED` policy: small guarded sampling, moderate quarantine, NUMA-local magazines. System daemons (parsers, media, net) can get `SECURE_STRICT`; low-latency UIs can get `LOW_LATENCY`.

---

## What this beats, concretely

- **Segment Heap (Windows):** NitroHeap keeps the footprint/concurrency benefits but adds trait-driven *movable allocations* and stronger default temporal safety + guard sampling.  
- **jemalloc:** Similar arena/extent sophistication, but more aggressive per-CPU magazines, built-in quarantine semantics, and kernel-visible partitions/contracts.  
- **PartitionAlloc:** Retains partition security but exposes traits system-wide (not just Chromium), adds NUMA-smart magazines and OS-level compaction for movable types.  
- **mimalloc:** Matches the ultra-fast cross-thread free path while adding stronger hardening and telemetry.  
- **ptmalloc/glibc:** Avoids in-band metadata and weak tcache patterns; stronger UAF resistance and lower contention under load.  
- **hardened_malloc:** Similar security posture (out-of-line metadata, pointer encoding), but with broader performance knobs (traits) and production guard-sampling built-in.

---

## Next steps
- Draft the `mallocx` flags map → NitroHeap traits, plus a minimal shim for existing malloc users.  
- Lay out the partition + magazine data structures and wire up the fast path state machine.  
- Define the `sys_heapctl` ABI and the per-partition stats record.
