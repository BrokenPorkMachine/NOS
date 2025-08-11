// Minimal public API shims around the internal loader.
// Provides the symbols used by regx.c and thread.c.
//
// This file is freestanding (no libc). It only depends on the kernel's
// serial printf and your internal load_agent() entry point.

#include <stdint.h>
#include <stddef.h>
#include <string.h>

// --- forward decls to avoid pulling any libc ---
// serial printf from kernel/printf.c
int serial_printf(const char *fmt, ...);

// internal loader entry (already implemented in kernel/agent_loader.c)
typedef enum {
    AGENT_FORMAT_ELF = 0,
    AGENT_FORMAT_MACHO2,
    AGENT_FORMAT_FLAT,
    AGENT_FORMAT_NOSM,
    AGENT_FORMAT_UNKNOWN
} agent_format_t;

int load_agent(const void *image, size_t size, agent_format_t fmt);

// default file reader exported by our tiny NOS filesystem facade
// (see kernel/nosfs_pub.c)
int nosfs_read_file(const char *path, void **out_buf, size_t *out_sz);

// --- public API expected by the rest of the tree ---
typedef int (*agent_read_cb_t)(const char *path, void **out_buf, size_t *out_sz);

// We don’t (yet) need to call into the security gate from here.
// Keep it as an opaque pointer so the signature can evolve without churn.
typedef void (*agent_gate_cb_t)(void);

static agent_read_cb_t g_read_cb = 0;
static agent_gate_cb_t g_gate_cb = 0;

// Simple format sniffer. Extend here if you add other container types.
static agent_format_t sniff_fmt(const void *img, size_t sz) {
    if (sz >= 4) {
        const unsigned char *p = (const unsigned char *)img;
        if (p[0] == 0x7f && p[1] == 'E' && p[2] == 'L' && p[3] == 'F')
            return AGENT_FORMAT_ELF;
    }
    return AGENT_FORMAT_UNKNOWN;
}

// Load with format autodetection.
int load_agent_auto(const void *image, size_t size) {
    agent_format_t fmt = sniff_fmt(image, size);
    if (fmt == AGENT_FORMAT_UNKNOWN) {
        serial_printf("[loader] unknown image magic; refusing (size=%zu)\n",
                      (unsigned long)size);
        return -38; // -ENOSYS/-ENODEV style; matches your logs
    }
    return load_agent(image, size, fmt);
}

// Run directly from a path (uses the registered reader or NOSFS).
int agent_loader_run_from_path(const char *path) {
    if (!path) return -1;

    if (!g_read_cb) g_read_cb = nosfs_read_file;

    void *buf = 0;
    size_t sz = 0;
    int rc = g_read_cb(path, &buf, &sz);
    if (rc < 0 || !buf || !sz) {
        serial_printf("[loader] run_from_path \"%s\" read failed rc=%d\n", path, rc);
        return rc ? rc : -5;
    }

    serial_printf("[loader] run_from_path \"%s\" size=%zu\n", path, (unsigned long)sz);
    return load_agent_auto(buf, sz);
}

// Allow regx/thread to override the path reader (eg. NOSFS, ramdisk, etc.)
void agent_loader_set_read(agent_read_cb_t cb) {
    g_read_cb = cb ? cb : nosfs_read_file;
}

// Allow regx to provide/own a security gate. Opaque here on purpose.
void agent_loader_set_gate(agent_gate_cb_t cb) {
    g_gate_cb = cb;
    // We don’t invoke it here; the gate is typically consulted by regx itself.
    // This keeps the shim side-effect free while satisfying the symbol.
}
