A. mallocx flags → NitroHeap trait map + a tiny shim for legacy code
Core data structures (partitions, magazines, slabs, size classes)
Fast-path state machines (alloc/free, same-CPU and cross-CPU)
sys_heapctl ABI + per-partition stats record

1) mallocx flags → NitroHeap traits (+ minimal shim)
1.1 Public flags (stable ABI)

// nitroheap_flags.h
#pragma once
#include <stdint.h>

// All flags are OR-combinable. Lower 16 bits are standardized.
// Upper bits reserved for NitrOS-specific extensions.

typedef uint64_t nh_flags_t;

// Security/temporal safety
#define NH_SECURE_STRICT     ((nh_flags_t)1u << 0)  // zero-on-alloc/free, short reuse epochs, high guard sampling
#define NH_SECURE_BALANCED   ((nh_flags_t)1u << 1)  // default hardened mode
#define NH_SECURE_RELAXED    ((nh_flags_t)1u << 2)  // larger magazines, longer reuse epochs

// Performance/latency
#define NH_LOW_LATENCY       ((nh_flags_t)1u << 3)  // prefer bump blocks, bigger magazines, minimal fences
#define NH_THROUGHPUT        ((nh_flags_t)1u << 4)  // bias to large refill batch sizes

// Placement / NUMA
#define NH_NUMA_LOCAL        ((nh_flags_t)1u << 5)  // allocate from local node only
#define NH_NUMA_INTERLEAVE   ((nh_flags_t)1u << 6)  // spread across nodes
#define NH_NUMA_HOTFOLLOW    ((nh_flags_t)1u << 7)  // magazines migrate to CPU/hot thread

// Object mobility and sharing
#define NH_MOVABLE           ((nh_flags_t)1u << 8)  // returns handle; allocator may relocate
#define NH_SHARED_RW         ((nh_flags_t)1u << 9)  // optimize for cross-thread frees

// Lifetime hints
#define NH_PERSISTENT        ((nh_flags_t)1u << 10) // hugepage-preferred, eager dirty->clean
#define NH_EPHEMERAL         ((nh_flags_t)1u << 11) // small slabs, fast MADV_DONTNEED

// Telemetry
#define NH_TELEM_SILENT      ((nh_flags_t)1u << 12) // opt-out of sampled stack capture

// Alignment (mutually exclusive low 4 bits of an alignment field)
#define NH_ALIGN_MASK        ((nh_flags_t)0xFULL << 48)
#define NH_ALIGN_LOG2_SHIFT  48                     // e.g., 12 => 4096 alignment
#define NH_ALIGN_LOG2(n)     (((nh_flags_t)(n)) << NH_ALIGN_LOG2_SHIFT)

// Size-class policy (rarely needed; override auto)
#define NH_CLASS_TINY        ((nh_flags_t)1u << 13)
#define NH_CLASS_SMALL       ((nh_flags_t)1u << 14)
#define NH_CLASS_MEDIUM      ((nh_flags_t)1u << 15)
#define NH_CLASS_LARGE       ((nh_flags_t)1u << 16)

// Reserved for future: bits 56..63
Recommended presets (for easy use)
#define NH_PRESET_BALANCED   (NH_SECURE_BALANCED | NH_NUMA_LOCAL)
#define NH_PRESET_HARDENED   (NH_SECURE_STRICT | NH_NUMA_LOCAL)
#define NH_PRESET_LOWLAT     (NH_LOW_LATENCY | NH_SECURE_RELAXED | NH_NUMA_HOTFOLLOW)
#define NH_PRESET_SERVER     (NH_THROUGHPUT | NH_PERSISTENT | NH_NUMA_INTERLEAVE)
///////////

1.2 Minimal shim API (drop-in for libc)
Expose jemalloc-style mallocx/rallocx/dallocx for flags, and keep plain malloc/free mapped to NH_PRESET_BALANCED.

// nitroheap_shim.h
#pragma once
#include <stddef.h>
#include <stdint.h>
#include "nitroheap_flags.h"

#ifdef __cplusplus
extern "C" {
#endif

void* malloc(size_t size);                       // default partition/policy
void  free(void* p);
void* calloc(size_t n, size_t sz);
void* realloc(void* p, size_t size);

// Extended API (flagged)
void* mallocx(size_t size, nh_flags_t flags);
int   dallocx(void* p, nh_flags_t flags);        // flags may carry alignment/lifetime hints for deferred ops
void* rallocx(void* p, size_t size, nh_flags_t flags);

// MOVABLE objects (optional)
typedef uint64_t nh_handle_t;
nh_handle_t halloc(size_t size, nh_flags_t flags);
void*       hptr(nh_handle_t h);                 // temporary direct pointer (epoch-scoped)
int         hfree(nh_handle_t h);

#ifdef __cplusplus
}
#endif

// nitroheap_shim.c  (minimal, production-safe; error handling omitted for brevity)
#include "nitroheap_shim.h"
#include "nitroheap_sys.h"    // syscalls (see §4)

static inline uint16_t pick_partition(nh_flags_t flags) {
  // Simple mapping: (flags → partition key). Kernel can virtualize real partitions per-process.
  // For first cut: hash traits into a small partition index space.
  uint64_t key = (flags & 0x0000FFFFFFFFFFFFull) ^ (flags >> 48);
  return (uint16_t)((key * 11400714819323198485ull) >> 49); // ~15-bit partition id
}

void* malloc(size_t size) {
  return mallocx(size, NH_PRESET_BALANCED);
}

void free(void* p) {
  dallocx(p, 0);
}

void* calloc(size_t n, size_t sz) {
  size_t bytes = n * sz; // TODO: overflow check
  void* p = mallocx(bytes, NH_PRESET_BALANCED | NH_SECURE_STRICT); // zero-on-alloc enforced by trait
  return p;
}

void* realloc(void* p, size_t size) {
  return rallocx(p, size, NH_PRESET_BALANCED);
}

void* mallocx(size_t size, nh_flags_t flags) {
  nh_alloc_req req = {
    .size = size,
    .flags = flags,
    .partition_hint = pick_partition(flags),
  };
  nh_alloc_resp resp;
  if (sys_nh_alloc(&req, &resp) != 0) return NULL;
  return resp.ptr;
}

int dallocx(void* p, nh_flags_t flags) {
  nh_free_req req = { .ptr = p, .flags = flags };
  return sys_nh_free(&req);
}

void* rallocx(void* p, size_t size, nh_flags_t flags) {
  nh_realloc_req req = { .ptr = p, .new_size = size, .flags = flags };
  nh_alloc_resp resp;
  if (sys_nh_realloc(&req, &resp) != 0) return NULL;
  return resp.ptr;
}

// MOVABLE
nh_handle_t halloc(size_t size, nh_flags_t flags) {
  nh_halloc_req req = { .size = size, .flags = flags | NH_MOVABLE };
  nh_halloc_resp resp;
  if (sys_nh_halloc(&req, &resp) != 0) return 0;
  return resp.handle;
}
void* hptr(nh_handle_t h) {
  nh_hptr_req req = { .handle = h };
  nh_alloc_resp resp;
  if (sys_nh_hptr(&req, &resp) != 0) return NULL;
  return resp.ptr; // ephemeral; invalidated across epochs or relocations
}
int hfree(nh_handle_t h) {
  nh_hfree_req req = { .handle = h };
  return sys_nh_hfree(&req);
}

2) Core allocator data structures
Notes: keep hot fields in the first cache line, align ring buffers to cache line size, and avoid false sharing across CPUs.

// nitroheap_core.h
#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>

#define NH_CACHELINE 64
#define NH_MAX_SIZE_CLASSES 72       // power-of-two + hand-picked gaps
#define NH_MAG_RING_SIZE    256      // per-CPU magazine ring for tiny/small
#define NH_PARTITION_NAME   32

// Forward decls
struct nh_slab;
struct nh_extent;

// Size class descriptor (read-mostly)
typedef struct nh_sizeclass {
  uint32_t size;            // user size
  uint16_t slot_size;       // internal size w/ header, color padding
  uint16_t slots_per_slab;
  uint16_t class_id;
  uint16_t flags;           // TINY/SMALL/MEDIUM/LARGE
} nh_sizeclass;

// Per-CPU magazine for a size class
typedef struct nh_magazine {
  _Atomic(uint32_t) head;               // ring head (index)
  _Atomic(uint32_t) tail;               // ring tail (index)
  void*             ring[NH_MAG_RING_SIZE]; // cached free objects
  uint32_t          class_id;
  uint32_t          cpu_id;
  _Atomic(uint32_t) remote_backlog;     // approximate count of remote frees
  char              pad[NH_CACHELINE - ((sizeof(_Atomic(uint32_t))*3 + sizeof(void*)*NH_MAG_RING_SIZE + 8) % NH_CACHELINE)];
} nh_magazine;

// Partition-global pools (per size class)
typedef struct nh_class_pool {
  _Atomic(struct nh_slab*) partial_list;   // lock-free list of partially used slabs
  _Atomic(struct nh_slab*) empty_list;     // empty slabs to refill magazines
  _Atomic(uint64_t)        pressure_score; // contention/fragmentation heuristic
} nh_class_pool;

// Partition traits (frozen at creation; can be updated via heapctl where allowed)
typedef struct nh_traits {
  uint64_t flags_mask;           // traits applied for this partition by default
  uint8_t  reuse_epoch_ticks;    // min ticks before reuse
  uint8_t  guard_sample_rate;    // 0..100 (percentage of allocs sampled)
  uint8_t  numa_policy;          // LOCAL/INTERLEAVE/HOTFOLLOW
  uint8_t  _reserved;
} nh_traits;

// Partition object (kernel visible)
typedef struct nh_partition {
  char                 name[NH_PARTITION_NAME];
  uint16_t             id;
  uint16_t             numa_node;
  nh_traits            traits;
  nh_class_pool        pools[NH_MAX_SIZE_CLASSES];
  _Atomic(uint64_t)    bytes_inuse;
  _Atomic(uint64_t)    bytes_committed;
  _Atomic(uint64_t)    allocs;
  _Atomic(uint64_t)    frees;
  // remote-free inbox per CPU per class (pointer stacks), elided for brevity
} nh_partition;

// Slab header (metadata out-of-line: this struct lives in shadow/metadata pages)
typedef struct nh_slab {
  nh_partition*        part;
  nh_sizeclass*        cls;
  _Atomic(uint32_t)    free_count;
  _Atomic(uint32_t)    bitmap_words;     // or pointer to bitmap
  void*                user_base;        // first user slot address (data page)
  struct nh_slab*      next;
} nh_slab;

// Per-CPU heap state
typedef struct nh_cpu_heap {
  uint32_t      cpu_id;
  nh_magazine   mags[NH_MAX_SIZE_CLASSES];
  // Epoch timing, harvest cursors, etc.
} nh_cpu_heap;

3) Fast-path state machines
3.1 Allocation (mallocx)
Inputs: size, flags → size class C, partition P, current CPU K.
Goal: O(1) path with no locks, single-producer/consumer on ring.
State machine (tiny/small/medium):
K.mags[C]: try pop from ring
if (head != tail): return ring[head++]
Harvest remote frees if remote_backlog over threshold: drain up to N items into ring (amortized) → goto 1.
Local slab fast refill
slab = pop(partition.pools[C].partial_list) (lock-free LIFO or ticket CAS)
Carve up to batch slots into magazine ring; update slab bitmap → goto 1.
Empty refill
If no partial slabs: get slab = pop(empty_list) or allocate new slab pages from VM
Initialize metadata in shadow; link to partial_list → goto 3.
Guarded sampling (probabilistic): redirect to guarded pool path with redzones and trap canaries; returns sampled pointer.
Large allocations (≥128 KiB):
Allocate an nh_extent with alignment hint (maybe hugepage); map via VM; return direct pointer.
Pseudocode (hot path):
void* nh_alloc_fast(size_t sz, nh_flags_t f) {
  class_id c = nh_class_from_size(sz);
  nh_magazine* m = &cpu_heap()->mags[c];
  uint32_t h = atomic_load_explicit(&m->head, memory_order_relaxed);
  uint32_t t = atomic_load_explicit(&m->tail, memory_order_acquire);
  if (h != t) {
    void* p = m->ring[h & (NH_MAG_RING_SIZE-1)];
    atomic_store_explicit(&m->head, h+1, memory_order_release);
    return p;
  }
  return nh_alloc_slow(sz, f, c, m);
}

3.2 Free (dallocx)
Same-CPU producer (fast):
If pointer’s home CPU == current CPU: push to local ring (tail++), or if full, batch-return to slab (partial_list).
Cross-CPU free:
Push pointer onto remote-free stack for the owner CPU (owner_mag.remote_stack.push(p)) using a single CAS.
Increment remote_backlog. Owning CPU drains opportunistically at step 2 of allocation.
Pseudocode (fast path):
void nh_free_fast(void* p) {
  meta = shadow_lookup(p);
  nh_magazine* m = owner_magazine(meta); // derived from slab->cls and cpu owner
  uint32_t t = atomic_load_explicit(&m->tail, memory_order_relaxed);
  uint32_t h = atomic_load_explicit(&m->head, memory_order_acquire);
  if ((t - h) < NH_MAG_RING_SIZE) {
    m->ring[t & (NH_MAG_RING_SIZE-1)] = p;
    atomic_store_explicit(&m->tail, t+1, memory_order_release);
    return;
  }
  nh_free_slow_overflow(m, p); // batch to slab; update lists
}
Temporal safety (epoch quarantine):
On batch return to slab, items enter an epoch queue; they are not eligible for allocation until reuse_epoch_ticks have elapsed for that CPU/partition.

4) sys_heapctl ABI + stats records
4.1 Syscall surface (user → kernel)
Keep a single multiplexed entry point for control (sys_heapctl) and a small set of hot syscalls for data path (sys_nh_alloc/free/...) that can be vDSO’d later.
// nitroheap_sys.h
#pragma once
#include <stddef.h>
#include <stdint.h>
#include "nitroheap_flags.h"

typedef enum {
  NH_HEAPCTL_CREATE_PARTITION = 1,
  NH_HEAPCTL_DESTROY_PARTITION,
  NH_HEAPCTL_SET_TRAITS,
  NH_HEAPCTL_GET_TRAITS,
  NH_HEAPCTL_SET_QUOTA,
  NH_HEAPCTL_GET_STATS,
  NH_HEAPCTL_SET_POLICY,          // process default flags/policy
  NH_HEAPCTL_ATTACH_PROCESS,      // bind process/thread to partition
  NH_HEAPCTL_DETACH_PROCESS,
  NH_HEAPCTL_DEBUG_SNAPSHOT,      // copies shadow metadata safely
} nh_heapctl_op;

typedef struct {
  uint16_t  part_id;
  char      name[32];
  nh_traits traits;
  uint64_t  quota_bytes;     // 0 = unlimited
} nh_heapctl_create_args;

typedef struct {
  uint16_t  part_id;
  nh_traits traits;          // fields respected per policy
} nh_heapctl_set_traits_args;

typedef struct {
  uint16_t  part_id;
  uint64_t  quota_bytes;
} nh_heapctl_set_quota_args;

typedef struct {
  uint32_t  pid;             // 0 == current process
  uint16_t  part_id;
} nh_heapctl_attach_args;

typedef struct {
  uint32_t  pid;             // 0 == current process
  nh_flags_t default_flags;  // process-default malloc policy
} nh_heapctl_set_policy_args;

// Stats (see §4.2)
typedef struct {
  uint16_t part_id;
  uint16_t include_per_cpu;  // bool
  void*    user_buf;         // where to copy results
  size_t   user_buf_len;
} nh_heapctl_get_stats_args;

int sys_heapctl(nh_heapctl_op op, const void* args, size_t args_len);

// Hot path syscalls (can be routed via vDSO in usermode for per-CPU fastpath)
typedef struct { size_t size; nh_flags_t flags; uint16_t partition_hint; } nh_alloc_req;
typedef struct { void*  ptr; } nh_alloc_resp;
typedef struct { void*  ptr; nh_flags_t flags; } nh_free_req;
typedef struct { void*  ptr; size_t new_size; nh_flags_t flags; } nh_realloc_req;

int sys_nh_alloc(const nh_alloc_req* in, nh_alloc_resp* out);
int sys_nh_free(const nh_free_req* in);
int sys_nh_realloc(const nh_realloc_req* in, nh_alloc_resp* out);

// MOVABLE
typedef struct { size_t size; nh_flags_t flags; } nh_halloc_req;
typedef struct { uint64_t handle; } nh_halloc_resp;
typedef struct { uint64_t handle; } nh_hptr_req;
typedef struct { uint64_t handle; } nh_hfree_req;

int sys_nh_halloc(const nh_halloc_req* in, nh_halloc_resp* out);
int sys_nh_hptr(const nh_hptr_req* in, nh_alloc_resp* out);
int sys_nh_hfree(const nh_hfree_req* in);

4.2 Per-partition stats record
Two tiers: summary (cheap, always on) and verbose (optional, per-CPU breakdown).
// nitroheap_stats.h
#pragma once
#include <stdint.h>

typedef struct {
  uint16_t part_id;
  char     name[32];
  uint64_t bytes_inuse;
  uint64_t bytes_committed;
  uint64_t allocs;
  uint64_t frees;
  uint64_t quarantine_backlog;   // items pending reuse epoch
  uint64_t remote_free_backlog;  // pending cross-CPU returns
  uint32_t guard_sample_rate;
  uint32_t reuse_epoch_ticks;
} nh_part_stats_summary;

typedef struct {
  uint16_t cpu_id;
  uint64_t allocs;
  uint64_t frees;
  uint64_t mag_hits;             // ring pops served
  uint64_t mag_misses;           // slowpath entries
  uint64_t harvests;             // remote drains
} nh_part_stats_cpu;

typedef struct {
  nh_part_stats_summary summary;
  uint32_t              n_cpus;
  // Followed by n_cpus entries of nh_part_stats_cpu (packed)
  // Followed by N size-class histograms (optional extension)
} nh_part_stats_blob;

sys_heapctl(NH_HEAPCTL_GET_STATS) semantics
If include_per_cpu = 0, copy nh_part_stats_summary only.
If include_per_cpu = 1, copy nh_part_stats_blob with n_cpus count, then array.
If user_buf_len is too small, return -ENOBUFS and set n_cpus you’d need.
Concurrency & memory-ordering notes (hot paths)
Magazine ring is single-producer/single-consumer from the owner CPU POV; cross-CPU frees never touch head/tail—only push to remote_stack (Treiber stack with ABA defense via version tag).
Ring operations: producer uses memory_order_release on tail, consumer uses memory_order_acquire on tail and release on head.
Remote harvest: drain up to N items under acquire on pop; batch push into ring with release on tail.
Suggested initial size classes
Tiny: 8, 16, 24, 32, 48, 64
Small: 80, 96, 112, 128, 160, 192, 224, 256, 320, 384, 448, 512
Medium: 640, 768, 896, 1k, 1.25k, 1.5k, 2k, 3k, 4k, 6k, 8k, 12k, 16k, 24k, 32k, 48k, 64k, 96k, 128k
Large: ≥128 KiB via extents
What you can implement next (in order)
Implement nh_class_from_size() and build the size-class table.
Wire per-CPU nh_magazine rings and remote-free stacks.
Implement slab carve/return and epoch quarantine queues.
Implement sys_nh_alloc/free/realloc stubs in kernel, with vDSO trampoline for userland fastpath (optional).
Add sys_heapctl handlers for CREATE/SET_TRAITS/GET_STATS and verify stats increments.
Add guarded sampling path (redirect Nth allocation to a protected pool with traps).
Introduce NH_MOVABLE handle table and background compactor.

  
