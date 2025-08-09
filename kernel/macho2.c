#include "macho2.h"

int macho2_load(const void *image, size_t size) {
    (void)image;
    (void)size;
    return 1; /* Stub: pretend success */
}

void *macho2_find_symbol(const void *image, size_t size, const char *name) {
    (void)image;
    (void)size;
    (void)name;
    return NULL;
}

