// kernel/agent_loader_pub.c
#include <stdint.h>
#include <stddef.h>
#include "agent_loader.h"

// Gate and reader/free hooks that regx/NOSFS will set.
static agent_gate_fn       g_gate   = 0;
static agent_read_file_fn  g_read   = 0;
static agent_free_fn       g_free   = 0;

// This is defined in agent_loader.c; we set it here.
extern int (*__agent_loader_spawn_fn)(const char *name, void *entry, int prio);

// ---- weak declarations so we can spawn without knowing the exact symbol name.
typedef void (*kentry0_t)(void);
__attribute__((weak)) int thread_spawn(kentry0_t entry, int prio, const char *name);
__attribute__((weak)) int thread_spawn_entry(kentry0_t entry, int prio, const char *name);
__attribute__((weak)) int spawn_kthread(kentry0_t entry, int prio, const char *name);

// Minimal, real spawner (used by the loader). Tries a few common symbols.
static int spawn_bridge(const char *name, void *entry, int prio)
{
    if (!entry) return -38; // ENOSYS-ish
    kentry0_t e = (kentry0_t)entry;

    if (thread_spawn)        return thread_spawn(e, prio, name ? name : "(anon)");
    if (thread_spawn_entry)  return thread_spawn_entry(e, prio, name ? name : "(anon)");
    if (spawn_kthread)       return spawn_kthread(e, prio, name ? name : "(anon)");
    return -38;
}

// Public API used by regx and thread.c
void agent_loader_set_gate(agent_gate_fn gate) { g_gate = gate; }

// If your NOSFS shim exports nosfs_read_file/nosfs_free, wire them here
extern int  nosfs_read_file(const char *path, const void **buf_out, size_t *sz_out);
extern void nosfs_free(void *p);

void agent_loader_set_read(agent_read_file_fn reader, agent_free_fn freer)
{
    // If regx wanted to override, keep that. Otherwise use the NOSFS defaults.
    g_read = reader ? reader : (agent_read_file_fn)nosfs_read_file;
    g_free = freer  ? freer  : (agent_free_fn)nosfs_free;

    // Ensure the loader will use our spawner from now on.
    __agent_loader_spawn_fn = spawn_bridge;
}

// Convenience so older call sites keep building.
int load_agent_auto(const void *img, size_t sz)
{
    return load_agent_with_prio(img, sz, AGENT_FORMAT_UNKNOWN, /*prio*/200);
}

// Read a path from /agents and load it with the requested prio.
// Also ensures the spawn hook is in place even if set_read() was never called.
int agent_loader_run_from_path(const char *path, int prio)
{
    if (!path) return -2;

    // Default hooks if regx didn't set them yet.
    if (!g_read) g_read = (agent_read_file_fn)nosfs_read_file;
    if (!g_free) g_free = (agent_free_fn)nosfs_free;
    if (!__agent_loader_spawn_fn) __agent_loader_spawn_fn = spawn_bridge;

    const void *buf = 0;
    size_t sz = 0;
    if (g_read(path, &buf, &sz) != 0 || !buf || !sz) return -5;

    // Let the core loader auto-detect the format and handle manifest/gate/spawn.
    int rc = load_agent_with_prio(buf, sz, AGENT_FORMAT_UNKNOWN, prio);

    // Free backing store if it came from NOSFS.
    if (g_free) g_free((void*)buf);
    return rc;
}

// Expose what the core loader expects to call when it wants to contact regx.
agent_gate_fn agent_loader_get_gate(void)
{
    return g_gate;
}
