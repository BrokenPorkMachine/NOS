#pragma once
#include <stdint.h>
#include "../VM/paging.h"
#include "../VM/pmm.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    void *addr;
    uint32_t size;
    uint32_t rights_send;
    uint32_t rights_recv;
} ipc_shared_mem_t;

int  ipc_shared_create(ipc_shared_mem_t *mem, uint32_t size, uint32_t send_mask, uint32_t recv_mask);
void ipc_shared_destroy(ipc_shared_mem_t *mem);
void *ipc_shared_map(ipc_shared_mem_t *mem);

#ifdef __cplusplus
}
#endif
