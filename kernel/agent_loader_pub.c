// kernel/agent_loader_pub.c  (drop-in)
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "agent_loader.h"   // agent_gate_fn, agent_read_file_fn, agent_free_fn, load_agent()
#include "nosfs_pub.h"      // nosfs_read_file()
extern int serial_printf(const char *fmt, ...);

/* ---------------- Gate hook ---------------- */
static agent_gate_fn g_gate = NULL;
void agent_loader_set_gate(agent_gate_fn gate) { g_gate = gate; }

/* ---------------- Read hook ---------------- */
static agent_read_file_fn g_reader = NULL;
static agent_free_fn      g_freeer = NULL;

void agent_loader_set_read(agent_read_file_fn reader, agent_free_fn freer) {
    g_reader = reader;
    g_freeer = freer;
}

/* ---------------- Auto-format detection ---------------- */
static agent_format_t detect_fmt(const void *img, size_t sz) {
    if (sz >= 4) {
        const unsigned char *p = (const unsigned char *)img;
        if (p[0] == 0x7f && p[1] == 'E' && p[2] == 'L' && p[3] == 'F')
            return AGENT_FORMAT_ELF;
    }
    return AGENT_FORMAT_UNKNOWN;
}

int load_agent_auto(const void *image, size_t size) {
    agent_format_t f = detect_fmt(image, size);
    if (f == AGENT_FORMAT_UNKNOWN) {
        /* fall back to ELF attempt; return error if it also doesn’t register */
        int rc = load_agent(image, size, AGENT_FORMAT_ELF);
        return rc ? rc : -1;
    }
    return load_agent(image, size, f);
}

/* ---------------- Path-based runner ---------------- */
int agent_loader_run_from_path(const char *path, int prio) {
    const void *buf = NULL;
    size_t sz = 0;

    if (g_reader) {
        if (g_reader(path, (void **)&buf, &sz) != 0) {
            serial_printf("[loader] run_from_path \"%s\" reader failed\n", path ? path : "(null)");
            return -1;
        }
    } else {
        if (nosfs_read_file(path, &buf, &sz) != 0) {
            serial_printf("[loader] run_from_path \"%s\" nosfs_read_file failed\n", path ? path : "(null)");
            return -1;
        }
    }

    serial_printf("[loader] run_from_path \"%s\" size=%zu prio=%d\n",
                  path ? path : "(null)", (unsigned long)sz, prio);

    int rc = load_agent_auto(buf, sz);

    if (g_freeer && buf) g_freeer((void *)buf);

    if (g_gate) g_gate(path, NULL, NULL, NULL, NULL);  // optional gate callback

    return rc;
}

/* ---------------- Spawn bridge expected by agent_loader.c ---------------- */
/* Provide a weak default for register_and_spawn; strong definition elsewhere will override. */
__attribute__((weak))
int register_and_spawn(const char *name, void *entry, int prio) {
    (void)name; (void)entry; (void)prio;
    return -38; /* ENOSYS-style fallback */
}

/* This global pointer is what agent_loader.c references. */
static int spawn_bridge(const char *name, void *entry, int prio) {
    return register_and_spawn(name, entry, prio);
}

/* Define the symbol with external linkage so the linker can resolve it. */
int (*__agent_loader_spawn_fn)(const char *name, void *entry, int prio) = spawn_bridge;
