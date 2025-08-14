#pragma once
#include <stddef.h>
#include <stdint.h>
void free(void *);   // declaration only; implementation comes from user/libc/libc.c

/* Returns 0 on success and fills *out and *out_sz with a read-only pointer to
   the file data and its size. The memory is static; do not free(). */
int nosfs_read_file(const char *path, const void **out, size_t *out_sz);
