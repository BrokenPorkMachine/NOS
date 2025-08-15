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
