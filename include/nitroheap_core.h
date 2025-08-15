#pragma once
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
