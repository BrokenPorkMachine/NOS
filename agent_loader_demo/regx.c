#include "regx.h"
#include <string.h>

#define REGX_MAX_ENTRIES 16

static regx_entry_t registry[REGX_MAX_ENTRIES];
static size_t registry_count = 0;
static uint64_t next_id = 1;

uint64_t regx_register(const regx_manifest_t *manifest, uint64_t parent_id) {
    if (registry_count >= REGX_MAX_ENTRIES) return 0;
    regx_entry_t *e = &registry[registry_count];
    e->id = next_id++;
    e->parent_id = parent_id;
    e->manifest = *manifest;
    registry_count++;
    return e->id;
}

const regx_entry_t *regx_query(uint64_t id) {
    for (size_t i = 0; i < registry_count; ++i) {
        if (registry[i].id == id)
            return &registry[i];
    }
    return NULL;
}

size_t regx_enumerate(const regx_selector_t *sel, regx_entry_t *out, size_t max) {
    size_t n = 0;
    for (size_t i = 0; i < registry_count && n < max; ++i) {
        if (sel && sel->type && sel->type[0]) {
            if (strcmp(registry[i].manifest.type, sel->type) != 0)
                continue;
        }
        out[n++] = registry[i];
    }
    return n;
}
