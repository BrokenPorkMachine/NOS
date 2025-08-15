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

// Recommended presets (for easy use)
#define NH_PRESET_BALANCED   (NH_SECURE_BALANCED | NH_NUMA_LOCAL)
#define NH_PRESET_HARDENED   (NH_SECURE_STRICT | NH_NUMA_LOCAL)
#define NH_PRESET_LOWLAT     (NH_LOW_LATENCY | NH_SECURE_RELAXED | NH_NUMA_HOTFOLLOW)
#define NH_PRESET_SERVER     (NH_THROUGHPUT | NH_PERSISTENT | NH_NUMA_INTERLEAVE)
