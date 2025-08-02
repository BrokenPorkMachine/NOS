#include "sharedmem.h"
#include "../../user/libc/libc.h"
#include "../VM/cow.h"

int ipc_shared_create(ipc_shared_mem_t *mem, uint32_t size, uint32_t send_mask, uint32_t recv_mask) {
    if (!mem || size == 0) return -1;
    uint32_t pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    void *base = alloc_page();
    if (!base) return -1;
    memset(base, 0, PAGE_SIZE);
    mem->addr = base;
    mem->size = pages * PAGE_SIZE;
    mem->rights_send = send_mask;
    mem->rights_recv = recv_mask;
    cow_inc_ref((uint64_t)base);
    return 0;
}

void ipc_shared_destroy(ipc_shared_mem_t *mem) {
    if (!mem || !mem->addr) return;
    cow_dec_ref((uint64_t)mem->addr);
    free_page(mem->addr);
    mem->addr = NULL;
}

void *ipc_shared_map(ipc_shared_mem_t *mem) {
    if (!mem || !mem->addr) return NULL;
    return mem->addr;
}
