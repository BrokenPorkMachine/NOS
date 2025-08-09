// src/agents/regx/regx.c
#include "../../user/libc/libc.h"   // adjust include if your libc path differs when linked into kernel
#include <stdint.h>
#include "../../kernel/Task/thread.h"
#include "../../kernel/arch/CPU/lapic.h"

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

static inline uint64_t rdtsc(void){
    uint32_t lo,hi; __asm__ volatile("rdtsc":"=a"(lo),"=d"(hi));
    return ((uint64_t)hi<<32)|lo;
}

static void watchdog_thread(void){
    for(;;){
        kprintf(".");
        for(volatile uint64_t i=0;i<1000000;i++) __asm__ volatile("pause");
        thread_yield();
    }
}

// ---- Security policy ----
// 1) Only allow files under /agents/ with .bin suffix
// 2) Require a non-empty name + entry
// 3) Capabilities must be a subset of a small allowlist
static int regx_policy_gate(const char *path,
                            const char *name,
                            const char *version,
                            const char *capabilities,
                            const char *entry)
{
    (void)version;

    uint32_t tid = thread_self();
    void *caller = __builtin_return_address(0);
    uint64_t ts = rdtsc();

    if (name && strcmp(name, "init") == 0)
        kprintf("[regx] allow call tid=%u pc=%p ts=%llu\n", tid, caller, ts);

    if (!path || strncmp(path, "/agents/", 8) != 0) {
        kprintf("[regx] deny: path %s outside /agents/\n", path?path:"(null)");
        return 0;
    }
    size_t L = strlen(path);
    if (L < 5 || strcmp(path + (L - 4), ".bin") != 0) {
        kprintf("[regx] deny: path %s not .bin\n", path);
        return 0;
    }
    if (!name || !name[0] || !entry || !entry[0]) {
        kprintf("[regx] deny: missing name/entry (path=%s)\n", path);
        return 0;
    }

    // Allowed capabilities (comma-separated): fs,net,pkg,upd,tty,gui
    const char *allowed[] = {"fs","net","pkg","upd","tty","gui"};
    if (capabilities && capabilities[0]) {
        // crude check: ensure every token appears in allowed list
        char caps[128];
        snprintf(caps, sizeof(caps), "%s", capabilities);
        char *p = caps;
        while (*p){
            char *tok = p;
            while (*p && *p != ',') p++;
            char saved = *p;
            *p = '\0';
            int ok = 0;
            for (size_t i=0;i<sizeof(allowed)/sizeof(allowed[0]);++i){
                if (strcmp(tok, allowed[i]) == 0){ ok = 1; break; }
            }
            if (!ok){
                kprintf("[regx] deny: cap \"%s\" not allowed for %s\n", tok, name);
                return 0;
            }
            if (saved == ',') p++; /* skip comma */
        }
    }

    kprintf("[regx] allow: %s (entry=%s caps=%s)\n", name, entry, capabilities?capabilities:"");
    return 1;
}

void regx_main(void){
    kprintf("[regx] security gate online\n");

    // Install gate so future agent loads (by init or others) are mediated
    agent_loader_set_gate(regx_policy_gate);

    // Kick off init as standalone; it will load the rest (subject to gate)
    thread_create_with_priority(watchdog_thread, 250);
    kprintf("[regx] launching init (boot:init:regx)\n");
    if (agent_loader_run_from_path("/agents/init.bin", 200) < 0) {
        kprintf("[regx] failed to launch /agents/init.bin\n");
    }

    // Idle loop; yield so other agents can run
    for(;;) thread_yield();
}
