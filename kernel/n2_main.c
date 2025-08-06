/**
 * N2 Kernel Main
 * ---------------
 * Minimal demonstration of the N2 agent–based kernel.  The kernel is
 * bootstrapped by the O2 UEFI boot agent which passes a manifest rich
 * `bootinfo_t` structure describing the loaded kernel and NOSM modules.
 *
 * This file shows how the kernel:
 *   - Initializes the runtime agent registry
 *   - Loads and sandboxes NOSM modules declared by the bootloader
 *   - Exposes a simple syscall table and agent discovery helpers
 *   - Supports dynamic loading/unloading of agents and hot‑reloading of
 *     a filesystem agent
 *
 * The implementation intentionally avoids any legacy monolithic design and
 * instead treats every service – even core drivers and filesystems – as an
 * independently versioned "agent".  Security is enforced through manifests
 * declaring capabilities and through runtime sandbox hooks (stubbed here).
 */

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "../boot/include/bootinfo.h"
#include "agent.h"
#include "nosm.h"

/* --- Syscall infrastructure -------------------------------------------- */
typedef long (*syscall_fn_t)(long,long,long,long,long,long);
static syscall_fn_t syscall_table[64];

/* Register a syscall at runtime.  The ABI is versioned and extensible. */
int n2_syscall_register(uint32_t num, syscall_fn_t fn) {
    if (num >= 64) return -1;
    syscall_table[num] = fn;
    return 0;
}

/* --- Sandboxing -------------------------------------------------------- */
/* In a real kernel this would configure MPU/MMU permissions, IPC handles, etc.
 * Here it is a stub documenting intent. */
static void sandbox_agent(const n2_agent_t *agent,
                          const bootinfo_module_t *mod) {
    (void)agent; (void)mod;
    /*
     * TODO: parse agent->manifest for capability and permission fields and
     *       enforce least‑privilege policies here.
     */
}

/* --- Module loading helpers ------------------------------------------- */
static int load_module(const bootinfo_module_t *m) {
    void *entry = nosm_load((void*)(uintptr_t)m->base, m->size);
    if (!entry)
        return -1;
    const n2_agent_t *agent = n2_agent_get(m->name);
    if (agent)
        sandbox_agent(agent, m);
    return 0;
}

int n2_load_agent_from_bootinfo(const bootinfo_module_t *m) {
    return load_module(m);
}

int n2_unload_agent(const char *name) {
    return n2_agent_unregister(name);
}

/* Hot‑reload a filesystem agent by unloading the old instance and loading
 * the replacement provided by userland. */
int n2_hot_reload_filesystem(const bootinfo_module_t *replacement) {
    /* Unregister the currently active filesystem agent and load the
     * replacement NOSFS module. The agent registry ensures that any
     * outstanding references are cleaned up before the new version is
     * published, providing hot‑swap safety. */
    n2_unload_agent("NOSFS");
    return load_module(replacement);
}

/* --- Agent discovery API ---------------------------------------------- */
/* Copy up to `max` agents into `out` for userland discovery. */
size_t n2_agent_enumerate(n2_agent_t *out, size_t max) {
    return n2_agent_list(out, max);
}

/* Trivial capability query: manifests are assumed to be UTF‑8 strings listing
 * capabilities separated by commas. */
const n2_agent_t *n2_agent_find_capability(const char *cap) {
    n2_agent_t tmp[N2_MAX_AGENTS];
    size_t n = n2_agent_list(tmp, N2_MAX_AGENTS);
    for (size_t i = 0; i < n; ++i) {
        const char *man = (const char *)tmp[i].manifest;
        if (man && cap && strstr(man, cap))
            return n2_agent_get(tmp[i].name);
    }
    return NULL;
}

/* --- Scheduler loop --------------------------------------------------- */
static void scheduler_loop(void) {
    while (1) {
        /* In a full kernel this would pick runnable agents/tasks.  We simply
         * spin here to illustrate the hand‑off to the scheduler. */
        __asm__("hlt");
    }
}

/* --- Entry point ------------------------------------------------------ */
void n2_main(bootinfo_t *bootinfo) {
    if (!bootinfo || bootinfo->magic != BOOTINFO_MAGIC_UEFI)
        return; /* invalid boot environment */

    n2_agent_registry_reset();

    for (uint32_t i = 0; i < bootinfo->module_count; ++i)
        load_module(&bootinfo->modules[i]);

    /* After all modules are loaded the kernel queries the registry for the
     * active filesystem agent.  The NOSFS module advertises the `filesystem`
     * capability in its manifest, allowing the kernel to bind to it without
     * hard‑coded names. */
    const n2_agent_t *fs = n2_agent_find_capability("filesystem");
    if (fs) {
        /* In a full kernel we would set up syscall vectors here to forward
         * VFS requests through fs->entry.  Keeping the reference allows us to
         * hot‑swap or unload the filesystem later. */
    }

    scheduler_loop();
}

/* Example static manifest for a hypothetical agent.  Real systems would use
 * JSON or CBOR; keeping it simple ensures the sample builds on any C toolchain.
 */
const char nosfs_manifest[] =
    "name=NOSFS,version=1.0.0,capabilities=filesystem,snapshot,rollback";

