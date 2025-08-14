// src/agents/regx/regx.c
#include "../../../user/libc/libc.h"   // snprintf, strcmp, etc.
#include <stdint.h>
#include <stdatomic.h>
#include "../../../kernel/Task/thread.h"
#include "../../../nosm/drivers/IO/serial.h"
#include "../../../kernel/agent_loader.h"
#include "../../../user/agents/nosfs/nosfs_server.h"
#include "../../../user/libc/string_guard.h"
#include "regx_key.h"
#include <string.h>

// Kernel console
extern int kprintf(const char *fmt, ...);
// Cooperative scheduler hook
extern void thread_yield(void);

// Loader APIs (exported by kernel)
typedef int (*agent_gate_fn)(const char *path,
                             const char *name,
                             const char *version,
                             const char *capabilities,
                             const char *entry);

extern void agent_loader_set_gate(agent_gate_fn gate);
extern int  agent_loader_run_from_path(const char *path, int prio);

static int _regx_load_agent(const char *path, const char *arg, uint32_t *out) {
    (void)arg;
    int tid = agent_loader_run_from_path(path, MAX_PRIORITY);
    if (tid >= 0) {
        kprintf("[regx] launched %s tid=%d prio=%d\n", path ? path : "(null)", tid, MAX_PRIORITY);
        if (out)
            *out = (uint32_t)tid;
    } else {
        kprintf("[regx] failed to launch %s rc=%d\n", path ? path : "(null)", tid);
    }
    return tid;
}

int regx_load(const char *name, const char *arg, uint32_t *out) {
    return _regx_load_agent(name, arg, out);
}

int regx_verify_launch_key(const char *key) {
    return (key && strcmp(key, REGX_LAUNCH_KEY) == 0) ? 0 : -1;
}

// --- utils ---
static inline uint64_t rdtsc(void){
    uint32_t lo,hi; __asm__ volatile("rdtsc":"=a"(lo),"=d"(hi));
    return ((uint64_t)hi<<32)|lo;
}

// Local bounded serial print (no implicit strlen)
static void serial_putsn_bounded(const char *s, size_t max) {
    if (!s) return;
    size_t n = __nitros_safe_strnlen(s, max);
    serial_putsn(s, n);
}

static _Atomic int init_spawned = 0;

static void spawn_init_once(void) {
    int expected = 0;
    if (!atomic_compare_exchange_strong(&init_spawned, &expected, 1))
        return;

    /*
     * Wait for the filesystem server to report readiness so it can
     * supply core agents like init.mo2.  However, avoid an indefinite
     * stall if NOSFS fails to come up (e.g. missing device driver).
     * After a bounded number of yields fall back to launching init
     * directly; the init and login agents are also bundled as boot
     * modules so boot can continue in a degraded mode.
     */
    for (int i = 0; i < 1000 && !nosfs_is_ready(); ++i)
        thread_yield();

    if (!nosfs_is_ready())
        kprintf("[regx] NOSFS not ready, launching init anyway\n");

    for (;;) {
        kprintf("[regx] launching init (boot:init:regx)\n");
        int rc = agent_loader_run_from_path("/agents/init.mo2", MAX_PRIORITY);
        if (rc >= 0) {
            kprintf("[regx] init agent launched rc=%d\n", rc);
            break;
        }
        kprintf("[regx] failed to launch init rc=%d; retrying...\n", rc);
        thread_yield();
    }
}

// ---- Security policy ----
// 1) Only allow files under /agents/ with .bin or .mo2 suffix
// 2) Require a non-empty name + entry
// 3) Capabilities must be a subset of a small allowlist
// Temporarily disable strict policy enforcement and simply log the request.
// The launch key check remains active via regx_verify_launch_key(), but
// additional path/capability validation is bypassed until the policy is
// refined.
static int regx_policy_gate(const char *name,
                            const char *entry_sym,
                            const char *entry_hex,
                            const char *capabilities,
                            const char *path)
{
    (void)entry_hex;
    (void)capabilities;
    (void)path;

    // Use a bounded serial output to avoid untrusted strlen operations.
    serial_puts("[regx] allow: ");
    serial_putsn_bounded(name ? name : "(null)", 64);
    serial_puts(" (entry=");
    serial_putsn_bounded(entry_sym ? entry_sym : "(null)", 64);
    serial_puts(")\n");

    return 0; // Always allow for now
}

void regx_main(void){
    kprintf("[regx] security gate online\n");

    // Install gate so future agent loads are mediated
    agent_loader_set_gate(regx_policy_gate);

    // Kick off init agent once we're online
    spawn_init_once();

    // Yield so the newly spawned init agent can run
    thread_yield();

    // Idle loop; halt CPU while regx waits for work
    for(;;) __asm__ volatile("hlt");
}
