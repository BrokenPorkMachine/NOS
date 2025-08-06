#include "regx.h"
#include <string.h>

static regx_entry_t entries[REGX_MAX_ENTRIES];
static size_t entry_count = 0;
static uint64_t next_id = 1;

uint64_t regx_register(const regx_manifest_t *m, uint64_t parent_id) {
    if (entry_count >= REGX_MAX_ENTRIES) return 0;
    regx_entry_t *e = &entries[entry_count++];
    e->id = next_id++;
    e->parent_id = parent_id;
    e->manifest = *m;
    return e->id;
}

const regx_entry_t *regx_query(uint64_t id) {
    for (size_t i = 0; i < entry_count; ++i)
        if (entries[i].id == id) return &entries[i];
    return NULL;
}

size_t regx_enumerate(const regx_selector_t *sel, regx_entry_t *out, size_t max) {
    size_t n = 0;
    for (size_t i = 0; i < entry_count && n < max; ++i) {
        const regx_entry_t *e = &entries[i];
        if (sel) {
            if (sel->type && e->manifest.type != sel->type) continue;
            if (sel->parent_id && e->parent_id != sel->parent_id) continue;
            if (sel->name_prefix[0] &&
                strncmp(e->manifest.name, sel->name_prefix, strlen(sel->name_prefix)) != 0)
                continue;
        }
        out[n++] = *e;
    }
    return n;
}

void regx_reset(void) {
    entry_count = 0;
    next_id = 1;
}
