#include <regx.h>
#include <string.h>
#include "Task/thread.h"
#include "uaccess.h"
#include <stdarg.h>

static void klog(const char *fmt, ...) {
    (void)fmt;
}

static bool regx_has_key(const char *module_name, const char *key) {
    (void)module_name;
    (void)key;
    return true;
}

static void regx_add_key(const char *module_name, const char *key, bool value) {
    (void)module_name;
    (void)key;
    (void)value;
}

static void regx_create_entry_storage(const char *module_name) {
    (void)module_name;
}

extern int kprintf(const char *fmt, ...);

typedef struct {
    volatile int locked;   // kept volatile + __sync to avoid broader refactors
    uint32_t owner;
} regx_lock_t;

static regx_lock_t regx_lock_obj = {0, 0};

static inline uint64_t rdtsc(void) {
    uint32_t lo, hi;
    __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

static void lock_acquire(const char *name) {
    uint64_t start = rdtsc();
    while (__sync_lock_test_and_set(&regx_lock_obj.locked, 1)) {
        if (rdtsc() - start > 100000000ULL) {
            kprintf("[regx] wait on %s by %u\n", name, thread_self());
            start = rdtsc();
        }
        __asm__ volatile("pause");
    }
    regx_lock_obj.owner = thread_self();
    kprintf("[regx] lock %s acquired by %u\n", name, regx_lock_obj.owner);
}

static void lock_release(const char *name) {
    kprintf("[regx] lock %s released by %u\n", name, thread_self());
    regx_lock_obj.owner = 0;
    __sync_lock_release(&regx_lock_obj.locked);
}

static void regx_lock(void) {
    lock_acquire("registry");
}

static void regx_unlock(void) {
    lock_release("registry");
}

// ---- Public hooks you likely already have ----
// extern void regx_lock(void);
// extern void regx_unlock(void);
// extern void klog(const char *fmt, ...);
// extern bool regx_has_key(const char *module_name, const char *key);
// extern void regx_add_key(const char *module_name, const char *key, bool value);
// extern void regx_create_entry_storage(const char *module_name); // your existing low-level entry creator

// ---- Minimal-boot setting: only check app.launch in early boot ----
static bool g_regx_minimal_boot_verification = true;

// Always give app.launch to these system modules
static void regx_force_core_launch_permission(const char *module_name) {
    static const char *core_modules[] = { "NOSM", "NOSFS", "INIT", NULL };

    for (const char **m = core_modules; *m; ++m) {
        if (strcmp(module_name, *m) == 0) {
            // Ensure we don't double-spam logs if called multiple times
            if (!regx_has_key(module_name, "app.launch")) {
                klog("[regx] Forcing app.launch permission for core module: %s", module_name);
                regx_add_key(module_name, "app.launch", true);
            }
            break;
        }
    }
}

// Create a registry entry and inject defaults for core modules
void regx_create_entry(const char *module_name) {
    regx_lock();
    regx_create_entry_storage(module_name);
    regx_force_core_launch_permission(module_name);
    regx_unlock();
}

// Verification used by the launcher. In minimal boot, only app.launch matters.
bool regx_verify(const char *module_name) {
    if (g_regx_minimal_boot_verification) {
        bool ok = regx_has_key(module_name, "app.launch");
        if (!ok) {
            klog("[regx] DENY: %s missing app.launch", module_name);
        }
        return ok;
    }

    // If you have a fuller policy for later, you can put it here.
    // return full_regx_policy_verify(module_name);
    return regx_has_key(module_name, "app.launch");
}

// Print proof to the log that app.launch exists for a module.
// If you have a key-iterator, call it here to dump more keys.
// This is intentionally tiny and safe for very early boot.
void regx_log_launch_key_status(const char *module_name) {
    bool present = regx_has_key(module_name, "app.launch");
    klog("[regx] %s.app.launch=%d", module_name, present ? 1 : 0);
}

// Optional: call this later (post-boot) if you want to restore full policy
void regx_disable_minimal_boot_verification(void) {
    g_regx_minimal_boot_verification = false;
}

static regx_entry_t regx_registry[REGX_MAX_ENTRIES];
static size_t regx_count = 0;
static uint64_t regx_next_id = 1;

uint64_t regx_register(const regx_manifest_t *m, uint64_t parent_id) {
    CANONICAL_GUARD(m);

    lock_acquire("registry");
    if (regx_count >= REGX_MAX_ENTRIES) {
        lock_release("registry");
        return 0;
    }

    // duplicate by (type,name)
    for (size_t i = 0; i < regx_count; ++i) {
        if (regx_registry[i].manifest.type == m->type &&
            strncmp(regx_registry[i].manifest.name, m->name,
                    sizeof(regx_registry[i].manifest.name)) == 0) {
            uint64_t id = regx_registry[i].id;
            lock_release("registry");
            return id;
        }
    }

    kprintf("[regx] entries before=%zu\n", regx_count);
    regx_entry_t *e = &regx_registry[regx_count++];
    memset(e, 0, sizeof(*e));
    e->id = regx_next_id++;
    e->parent_id = parent_id;

    strlcpy(e->manifest.name,     m->name,     sizeof(e->manifest.name));
    e->manifest.type = m->type;
    strlcpy(e->manifest.version,  m->version,  sizeof(e->manifest.version));
    strlcpy(e->manifest.abi,      m->abi,      sizeof(e->manifest.abi));
    strlcpy(e->manifest.capabilities, m->capabilities,
             sizeof(e->manifest.capabilities));

    kprintf("[regx] entries after=%zu\n", regx_count);
    lock_release("registry");
    return e->id;
}

int regx_unregister(uint64_t id) {
    lock_acquire("registry");
    for (size_t i = 0; i < regx_count; ++i) {
        if (regx_registry[i].id == id) {
            regx_registry[i] = regx_registry[--regx_count]; // swap-remove
            lock_release("registry");
            return 0;
        }
    }
    lock_release("registry");
    return -1;
}

const regx_entry_t *regx_query(uint64_t id) {
    lock_acquire("registry");
    const regx_entry_t *ret = NULL;
    for (size_t i = 0; i < regx_count; ++i) {
        if (regx_registry[i].id == id) {
            ret = &regx_registry[i];
            break;
        }
    }
    lock_release("registry");
    return ret;
}

size_t regx_enumerate(const regx_selector_t *sel, regx_entry_t *out, size_t max) {
    CANONICAL_GUARD(sel);
    CANONICAL_GUARD(out);
    if (!out || max == 0) return 0;

    size_t n = 0;
    const char *prefix = NULL;
    size_t prefix_len = 0;

    if (sel && sel->name_prefix[0]) {
        prefix = sel->name_prefix;
        prefix_len = strnlen(prefix, sizeof(sel->name_prefix));
    }

    lock_acquire("registry");
    for (size_t i = 0; i < regx_count && n < max; ++i) {
        if (sel) {
            if (sel->type && regx_registry[i].manifest.type != sel->type)
                continue;
            if (sel->parent_id && regx_registry[i].parent_id != sel->parent_id)
                continue;
            if (prefix_len &&
                strncmp(regx_registry[i].manifest.name, prefix, prefix_len) != 0)
                continue;
        }
        out[n++] = regx_registry[i];
    }
    lock_release("registry");
    return n;
}
