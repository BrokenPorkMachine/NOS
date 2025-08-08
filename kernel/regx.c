#include <regx.h>
#include <string.h>

static regx_entry_t regx_registry[REGX_MAX_ENTRIES];
static size_t regx_count = 0;
static uint64_t regx_next_id = 1;

uint64_t regx_register(const regx_manifest_t *m, uint64_t parent_id) {
    if (regx_count >= REGX_MAX_ENTRIES)
        return 0;
    regx_entry_t *e = &regx_registry[regx_count++];
    e->id = regx_next_id++;
    e->parent_id = parent_id;
    e->manifest = *m;
    return e->id;
}

int regx_unregister(uint64_t id) {
    for (size_t i = 0; i < regx_count; ++i) {
        if (regx_registry[i].id == id) {
            regx_registry[i] = regx_registry[--regx_count];
            return 0;
        }
    }
    return -1;
}

const regx_entry_t *regx_query(uint64_t id) {
    for (size_t i = 0; i < regx_count; ++i)
        if (regx_registry[i].id == id)
            return &regx_registry[i];
    return NULL;
}

size_t regx_enumerate(const regx_selector_t *sel, regx_entry_t *out, size_t max) {
    if (!out || max == 0)
        return 0;

    size_t n = 0;
    const char *prefix = NULL;
    size_t prefix_len = 0;

    if (sel && sel->name_prefix[0]) {
        prefix = sel->name_prefix;
        while (prefix_len < sizeof(sel->name_prefix) && prefix[prefix_len])
            prefix_len++;
    }

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
    return n;
}
