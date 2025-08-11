// kernel/agent_loader_pub.c
// Public loader API shim with zero hard dependency on NOSFS.
// regx / threads must call agent_loader_set_read() to provide a reader.

#include <stdint.h>
#include <stddef.h>

// serial printf from kernel/printf.c
int serial_printf(const char *fmt, ...);

// internal loader entry (already in kernel/agent_loader.c)
typedef enum {
    AGENT_FORMAT_ELF = 0,
    AGENT_FORMAT_MACHO2,
    AGENT_FORMAT_FLAT,
    AGENT_FORMAT_NOSM,
    AGENT_FORMAT_UNKNOWN
} agent_format_t;

int load_agent(const void *image, size_t size, agent_format_t fmt);

// API surface expected by regx/thread
typedef int (*agent_read_cb_t)(const char *path, void **out_buf, size_t *out_sz);
typedef void (*agent_gate_cb_t)(void);

static agent_read_cb_t g_read_cb = 0;
static agent_gate_cb_t g_gate_cb = 0;

// Tiny format sniffer (extend as needed)
static agent_format_t sniff_fmt(const void *img, size_t sz) {
    if (sz >= 4) {
        const unsigned char *p = (const unsigned char *)img;
        if (p[0] == 0x7f && p[1] == 'E' && p[2] == 'L' && p[3] == 'F')
            return AGENT_FORMAT_ELF;
    }
    return AGENT_FORMAT_UNKNOWN;
}

int load_agent_auto(const void *image, size_t size) {
    agent_format_t fmt = sniff_fmt(image, size);
    if (fmt == AGENT_FORMAT_UNKNOWN) {
        serial_printf("[loader] unknown image magic; size=%zu\n", (unsigned long)size);
        return -38; // ENOSYS-ish
    }
    return load_agent(image, size, fmt);
}

int agent_loader_run_from_path(const char *path) {
    if (!path) return -1;
    if (!g_read_cb) {
        serial_printf("[loader] no path reader registered for \"%s\"\n", path);
        return -38; // tell caller we cannot read paths
    }

    void *buf = 0;
    size_t sz = 0;
    int rc = g_read_cb(path, &buf, &sz);
    if (rc < 0 || !buf || !sz) {
        serial_printf("[loader] read \"%s\" failed rc=%d\n", path, rc);
        return rc ? rc : -5;
    }

    serial_printf("[loader] run_from_path \"%s\" size=%zu\n", path, (unsigned long)sz);
    return load_agent_auto(buf, sz);
}

void agent_loader_set_read(agent_read_cb_t cb) {
    g_read_cb = cb;
}

void agent_loader_set_gate(agent_gate_cb_t cb) {
    g_gate_cb = cb;
    (void)g_gate_cb; // reserved; no-op here
}
