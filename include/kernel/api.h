#ifndef N2_KERNEL_API_H
#define N2_KERNEL_API_H

#include <stddef.h>
#include <stdint.h>

int api_puts(const char *s);
int api_fs_read_all(const char *path, void *buf, size_t len, size_t *outlen);
int api_regx_load(const char *name, const char *arg, uint32_t *out);
void api_yield(void);

#endif /* N2_KERNEL_API_H */
