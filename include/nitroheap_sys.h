#pragma once
#include <stddef.h>
#include <stdint.h>
#include "nitroheap_flags.h"
#include "nitroheap_core.h"

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
