// kernel/agent_loader_pub.c
#include <stddef.h>
#include <stdint.h>
#include "agent_loader.h"

// Exported hook the core loader uses to spawn the agent thread.
// RegX (or your thread layer) should set this at boot.
int (*__agent_loader_spawn_fn)(const char *name, void *entry, int prio) = 0;

// Current gate + I/O callbacks
static agent_gate_fn        g_gate  = 0;
static agent_read_file_fn   g_read  = 0;
static agent_free_fn        g_free  = 0;

// Minimal serial proto for freestanding builds.
// (Provided by serial.c; we only need the decl here.)
int  serial_printf(const char *fmt, ...);

// Weak defaults from NOSFS shim (kernel/nosfs_pub.c).
// If they’re linked, we adopt them automatically.
__attribute__((weak)) int  nosfs_read_file(const char *path, const void **data_out, size_t *size_out);
__attribute__((weak)) void nosfs_free(void *p);

static void ensure_default_reader(void) {
    if (!g_read && nosfs_read_file) {
        g_read = nosfs_read_file;
        g_free = nosfs_free;
    }
}

// --- Gate API --------------------------------------------------------------
void agent_loader_set_gate(agent_gate_fn gate) { g_gate = gate; }
agent_gate_fn agent_loader_get_gate(void)      { return g_gate; }

// --- Reader API ------------------------------------------------------------
void agent_loader_set_read(agent_read_file_fn reader, agent_free_fn freer) {
    g_read = reader;
    g_free = freer;
}

// --- Path loader -----------------------------------------------------------
// Read an agent image from filesystem and delegate to the core loader.
// Returns loader rc; -38 if no reader is configured.
int agent_loader_run_from_path(const char *path, int prio) {
    ensure_default_reader();

    if (!g_read) {
        serial_printf("[loader] no reader configured; can't open \"%s\"\n",
                      path ? path : "(null)");
        return -38; // match existing rc used in logs
    }

    const void *data = 0;
    size_t size = 0;
    int rc = g_read(path, &data, &size);
    if (rc != 0) {
        serial_printf("[loader] read failed path=\"%s\" rc=%d\n",
                      path ? path : "(null)", rc);
        return rc;
    }

    rc = load_agent_with_prio(data, size, AGENT_FORMAT_ELF, prio);

    if (g_free && data) g_free((void*)data);
    return rc;
}
