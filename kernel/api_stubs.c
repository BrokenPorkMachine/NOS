#include <stddef.h>
#include <stdint.h>

// Bring in prototypes for your kernel logging, filesystem, regx, and scheduler
#include "printf.h"       // for kprintf or kernel_puts
#include "fs.h"           // for fs_read_all
#include "regx.h"         // for regx_load
#include "thread.h"       // for thread_yield or scheduler_yield

// Agent API function implementations

int api_puts(const char *s) {
    // Simple kernel log wrapper
    kprintf("%s", s);
    return 0;
}

int api_fs_read_all(const char *path, void *buf, size_t len, size_t *outlen) {
    // Assumes your kernel has fs_read_all or similar
    return fs_read_all(path, buf, len, outlen);
}

int api_regx_load(const char *name, const char *arg, uint32_t *out) {
    // Assumes regx_load loads a registry entry / agent
    return regx_load(name, arg, out);
}

void api_yield(void) {
    // Yield current thread
    thread_yield();
}
