#include <stdint.h>
#include <stddef.h>
#include "libc.h"
#include "VM/paging_adv.h"

extern size_t thread_struct_size;

void *alloc_thread_struct(void) {
    return calloc(1, thread_struct_size);
}

void *alloc_stack(size_t size, int user_mode) {
    size_t pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    uint8_t *mem = malloc((pages + 2) * PAGE_SIZE + PAGE_SIZE);
    if (!mem)
        return NULL;
    uintptr_t aligned = ((uintptr_t)mem + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    paging_unmap_adv(aligned);
    paging_unmap_adv(aligned + (pages + 1) * PAGE_SIZE);
    uint8_t *base = (uint8_t *)aligned + PAGE_SIZE;
    if (user_mode) {
        // Placeholder for stack randomization; rand() not available in freestanding build
        uintptr_t rnd = 0;
        base += rnd;
    }
    return base + pages * PAGE_SIZE;
}
