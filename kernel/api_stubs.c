#include <stddef.h>
#include <stdint.h>

// ---- Forward declarations for the actual kernel functions ----
// Logging
int kprintf(const char *fmt, ...);

// Filesystem
int fs_read_all(const char *path, void *buf, size_t len, size_t *outlen);

// Registry/agent loader
int regx_load(const char *name, const char *arg, uint32_t *out);

// Thread scheduler
void thread_yield(void);

// ---- Agent API shims ----
int api_puts(const char *s) {
    kprintf("%s", s);
    return 0;
}

int api_fs_read_all(const char *path, void *buf, size_t len, size_t *outlen) {
    return fs_read_all(path, buf, len, outlen);
}

int api_regx_load(const char *name, const char *arg, uint32_t *out) {
    return regx_load(name, arg, out);
}

void api_yield(void) {
    thread_yield();
}
