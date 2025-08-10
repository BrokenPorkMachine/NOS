// src/agents/regx/regx.c
#include "../../user/libc/libc.h"   // snprintf, strcmp, etc.
#include <stdint.h>
#include <stdatomic.h>
#include "../../kernel/Task/thread.h"
#include "../../kernel/arch/CPU/lapic.h"
#include "drivers/IO/serial.h"
extern void serial_putc(char c);  // <-- ADD THIS

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

// --- utils ---
static inline uint64_t rdtsc(void){
    uint32_t lo,hi; __asm__ volatile("rdtsc":"=a"(lo),"=d"(hi));
    return ((uint64_t)hi<<32)|lo;
}

static inline int is_canonical(const void *p) {
    uintptr_t x = (uintptr_t)p;
    return ((x >> 48) == 0) || ((x >> 48) == 0xFFFF);
}

// Bounded strlen that never scans past `max`; refuses non-canonical pointers.
static size_t safe_strnlen(const char *s, size_t max) {
    if (!s || !is_canonical(s)) return 0;
    size_t n = 0;
    while (n < max) { char c = s[n]; if (!c) break; n++; }
    return n;
}

// Always NUL-terminates; never reads more than cap-1 bytes.
static void strnzcpy_cap(char *dst, const char *src, size_t cap) {
    if (!dst || cap == 0) return;
    if (!src || !is_canonical(src)) { dst[0] = 0; return; }
    size_t n = safe_strnlen(src, cap - 1);
    for (size_t i = 0; i < n; ++i) dst[i] = src[i];
    dst[n] = 0;
}

// Local bounded serial print (no implicit strlen)
static void serial_putsn_bounded(const char *s, size_t max) {
    if (!s) return;
    size_t n = safe_strnlen(s, max);
    for (size_t i = 0; i < n; ++i) serial_putc(s[i]);
}

static _Atomic int init_spawned = 0;

static void spawn_init_once(void) {
    int expected = 0;
    if (!atomic_compare_exchange_strong(&init_spawned, &expected, 1))
        return;
    kprintf("[regx] launching init (boot:init:regx)\n");
    if (agent_loader_run_from_path("/agents/init.bin", 200) < 0)
        kprintf("[regx] failed to launch /agents/init.bin\n");
}

static void watchdog_thread(void){
    for(;;){
        serial_write('.');
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

    // De-dupe "init" spammy log across very short window
    static _Atomic uint64_t last_init_log = 0;
    if (name && is_canonical(name) && strcmp(name, "init") == 0) {
        uint64_t prev = atomic_load(&last_init_log);
        if (ts - prev > 1000000ULL) {
            kprintf("[regx] allow call tid=%u pc=%p ts=%llu\n", tid, caller, ts);
            atomic_store(&last_init_log, ts);
        }
    }

    // Validate pointers before any string ops
    if (!is_canonical(path) || !is_canonical(name) || !is_canonical(entry) ||
        (capabilities && !is_canonical(capabilities))) {
        kprintf("[regx] deny: bad pointer(s) path=%p name=%p entry=%p caps=%p\n",
                path, name, entry, capabilities);
        return 0;
    }

    size_t path_len = safe_strnlen(path, 256);
    if (path_len < 8 || strncmp(path, "/agents/", 8) != 0) {
        kprintf("[regx] deny: path %s outside /agents/\n", path?path:"(null)");
        return 0;
    }
    if (path_len < 5 || strcmp(path + (path_len - 4), ".bin") != 0) {
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
        char caps[128];
        strnzcpy_cap(caps, capabilities, sizeof(caps)); // bounded copy-in
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
            if (saved == ',') p++;
        }
    }

    serial_puts("[regx] allow: ");
    serial_putsn_bounded(name, 64);
    serial_puts(" (entry=");
    serial_putsn_bounded(entry, 64);
    serial_puts(" caps=");
    if (capabilities && capabilities[0]) serial_putsn_bounded(capabilities, 64);
    serial_puts(")\n");
    return 1;
}

void regx_main(void){
    kprintf("[regx] security gate online\n");

    // Install gate so future agent loads are mediated
    agent_loader_set_gate(regx_policy_gate);

    // Kick off init (one-shot). Watchdog just keeps the core responsive.
    thread_create_with_priority(watchdog_thread, 250);
    spawn_init_once();

    // Idle loop; yield so other agents can run
    for(;;) thread_yield();
}
