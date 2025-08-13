// src/agents/regx/regx.c
#include "../../../user/libc/libc.h"   // snprintf, strcmp, etc.
#include <stdint.h>
#include <stdatomic.h>
#include "../../../kernel/Task/thread.h"
#include "../../../nosm/drivers/IO/serial.h"
#include "../../../kernel/agent_loader.h"
#include "../../../kernel/init_bin.h"
#include "../../../user/agents/nosfs/nosfs_server.h"
#include "../../../user/libc/string_guard.h"

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
    int tid = agent_loader_run_from_path(path, 200);
    if (tid >= 0 && out) {
        *out = (uint32_t)tid;
    }
    return tid;
}

int regx_load(const char *name, const char *arg, uint32_t *out) {
    return _regx_load_agent(name, arg, out);
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

    // Wait until the filesystem server has preloaded core agents.
    while (!nosfs_is_ready())
        thread_yield();

    for (;;) {
        kprintf("[regx] launching init (boot:init:regx)\n");
        int rc = agent_loader_run_from_path("/agents/init.mo2", 200);
        if (rc < 0) {
        kprintf("[regx] falling back to built-in init image\n");
        rc = load_agent_auto(init_bin, init_bin_len);
        }
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
static int regx_policy_gate(const char *name,
                            const char *entry_sym,
                            const char *entry_hex,
                            const char *capabilities,
                            const char *path)
{
    (void)entry_sym;
    (void)entry_hex;

    uint32_t tid = thread_self();
    void *caller = __builtin_return_address(0);
    uint64_t ts = rdtsc();

    // De-dupe "init" spammy log across very short window
    static _Atomic uint64_t last_init_log = 0;
    if (name && __nitros_is_canonical_ptr(name) && strcmp(name, "init") == 0) {
        uint64_t prev = atomic_load(&last_init_log);
        if (ts - prev > 1000000ULL) {
            kprintf("[regx] allow call tid=%u pc=%p ts=%llu\n", tid, caller, ts);
            atomic_store(&last_init_log, ts);
        }
    }

    // Validate pointers before any string ops
    if (!__nitros_is_canonical_ptr(path) || !__nitros_is_canonical_ptr(name) ||
        (capabilities && !__nitros_is_canonical_ptr(capabilities))) {
        kprintf("[regx] deny: bad pointer(s) path=%p name=%p caps=%p\n",
                path, name, capabilities);
        return -1;
    }

    size_t path_len = __nitros_safe_strnlen(path, 256);
    if (path && strcmp(path, "(buffer)") != 0) {
        if (path_len < 8 || strncmp(path, "/agents/", 8) != 0) {
            kprintf("[regx] deny: path %s outside /agents/\n", path ? path : "(null)");
            return -1;
        }
        if (path_len < 5 || (strcmp(path + (path_len - 4), ".bin") != 0 &&
                             strcmp(path + (path_len - 4), ".mo2") != 0)) {
            kprintf("[regx] deny: path %s not .bin/.mo2\n", path);
            return -1;
        }
    }
    if (!name || !name[0] || !entry_sym || !entry_sym[0]) {
        kprintf("[regx] deny: missing name/entry (path=%s)\n", path ? path : "(null)");
        return -1;
    }

    // Allowed capabilities (comma-separated): fs,net,pkg,upd,tty,gui
    const char *allowed[] = {"fs","net","pkg","upd","tty","gui"};
    if (capabilities && capabilities[0]) {
        char caps[128];
        strlcpy(caps, capabilities, sizeof(caps)); // bounded copy-in
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
                return -1;
            }
            if (saved == ',') p++;
        }
    }

    serial_puts("[regx] allow: ");
    serial_putsn_bounded(name, 64);
    serial_puts(" (entry=");
    serial_putsn_bounded(entry_sym, 64);
    serial_puts(" caps=");
    if (capabilities && capabilities[0]) serial_putsn_bounded(capabilities, 64);
    serial_puts(" path=");
    if (path && path[0]) serial_putsn_bounded(path, 64);
    serial_puts(")\n");
    return 0;
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
