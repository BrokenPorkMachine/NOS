#include "../include/regx.h"
#include <string.h>

/* Simple spinlock for atomic operations.  Real kernel would use arch‑specific
 * primitives. */
typedef int spinlock_t;
static spinlock_t regx_lock;
static void lock(spinlock_t *l)   { (void)l; /* stub */ }
static void unlock(spinlock_t *l) { (void)l; }

static regx_entry_t registry[REGX_MAX_ENTRIES];
static size_t registry_count;
static uint64_t next_id = 1; /* 0 reserved */

/* Atomic registration.  The entry is copied so callers can free their
 * manifest/signature afterwards.  Security checks (signatures, permissions)
 * would occur before insertion. */
int regx_register(const regx_entry_t *entry) {
    if (!entry)
        return -1;
    lock(&regx_lock);
    if (registry_count >= REGX_MAX_ENTRIES) {
        unlock(&regx_lock);
        return -1;
    }
    regx_entry_t *dst = &registry[registry_count];
    memcpy(dst, entry, sizeof(*dst));
    dst->id = next_id++;
    dst->generation = 1;
    registry_count++;
    unlock(&regx_lock);
    return 0;
}

/* Atomic removal of an entry by ID. */
int regx_unregister(uint64_t id) {
    lock(&regx_lock);
    for (size_t i = 0; i < registry_count; ++i) {
        if (registry[i].id == id) {
            memmove(&registry[i], &registry[i + 1],
                    (registry_count - i - 1) * sizeof(regx_entry_t));
            registry_count--;
            unlock(&regx_lock);
            return 0;
        }
    }
    unlock(&regx_lock);
    return -1;
}

/* Update selected fields atomically.  Only runtime state, parent or private
 * data are mutable; manifest and ID remain constant. */
int regx_update(uint64_t id, const regx_entry_t *delta) {
    if (!delta)
        return -1;
    lock(&regx_lock);
    for (size_t i = 0; i < registry_count; ++i) {
        if (registry[i].id == id) {
            registry[i].state = delta->state ? delta->state : registry[i].state;
            registry[i].parent_id =
                delta->parent_id ? delta->parent_id : registry[i].parent_id;
            registry[i].priv = delta->priv ? delta->priv : registry[i].priv;
            registry[i].generation++;
            unlock(&regx_lock);
            return 0;
        }
    }
    unlock(&regx_lock);
    return -1;
}

/* Lookup by ID.  Caller receives a const pointer; modifications require
 * regx_update. */
const regx_entry_t *regx_query(uint64_t id) {
    lock(&regx_lock);
    for (size_t i = 0; i < registry_count; ++i) {
        if (registry[i].id == id) {
            const regx_entry_t *ret = &registry[i];
            unlock(&regx_lock);
            return ret;
        }
    }
    unlock(&regx_lock);
    return NULL;
}

/* Enumerate entries matching the selector.  Results are copied out to avoid
 * exposing internal state. */
size_t regx_enumerate(const regx_selector_t *sel,
                      regx_entry_t *out, size_t max) {
    if (!out || !max)
        return 0;
    size_t count = 0;
    lock(&regx_lock);
    for (size_t i = 0; i < registry_count && count < max; ++i) {
        regx_entry_t *e = &registry[i];
        if (sel) {
            if (sel->type && e->manifest.type != sel->type)
                continue;
            if (sel->parent_id && e->parent_id != sel->parent_id)
                continue;
            if (sel->capability &&
                !strstr(e->manifest.capabilities, sel->capability))
                continue;
        }
        out[count++] = *e;
    }
    unlock(&regx_lock);
    return count;
}
