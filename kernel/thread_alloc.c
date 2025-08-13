#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>

extern size_t thread_struct_size;

void *alloc_thread_struct(void) {
    return calloc(1, thread_struct_size);
}

void *alloc_stack(size_t size, int user_mode) {
    (void)user_mode;
    uint8_t *mem = malloc(size);
    if (!mem)
        return NULL;
    return mem + size;
}
