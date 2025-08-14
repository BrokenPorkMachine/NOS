#include <stddef.h>
#include <stdint.h>
#include "klib/stdlib.h"
#include "nosfs_pub.h"

/* Forward declaration provided by nosfs.c */
extern int fs_read_all(const char *path, void **out, size_t *out_sz);

/* Public NOSFS read API used by the loader.  This now delegates to the
   in-memory NOSFS implementation instead of relying on built-in stubs. */
int nosfs_read_file(const char *path, const void **out, size_t *out_sz)
{
    if (!out || !out_sz || !path)
        return -1;

    void *buf = NULL;
    size_t sz = 0;
    int rc = fs_read_all(path, &buf, &sz);
    if (rc != 0)
        return rc;

    *out    = buf;
    *out_sz = sz;
    return 0;
}

/* Matching free helper so agent_loader can release buffers. */
void nosfs_free(void *p)
{
    free(p);
}
