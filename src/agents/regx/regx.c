#include "regx.h"
#include <string.h>
#include <stddef.h>
#include <stdio.h>

static regx_entry_t regx_registry[REGX_MAX_ENTRIES];
static size_t regx_count = 0;
static uint64_t regx_next_id = 1;

uint64_t regx_register(const regx_entry_t *entry) {
    if (regx_count >= REGX_MAX_ENTRIES) return 0;
    regx_entry_t *e = &regx_registry[regx_count++];
    *e = *entry;
    e->id = regx_next_id++;
    e->generation = 1;
    return e->id;
}

int regx_unregister(uint64_t id) {
    for (size_t i=0; i<regx_count; ++i) {
        if (regx_registry[i].id == id) {
            regx_registry[i] = regx_registry[--regx_count];
            return 0;
        }
    }
    return -1;
}

int regx_update(uint64_t id, const regx_entry_t *delta) {
    for (size_t i = 0; i < regx_count; ++i) {
        if (regx_registry[i].id == id) {
            regx_entry_t *e = &regx_registry[i];
            e->parent_id = delta->parent_id;
            e->state = delta->state;
            e->signature = delta->signature;
            e->generation++;
            return 0;
        }
    }
    return -1;
}

size_t regx_enumerate(const regx_selector_t *sel, regx_entry_t *out, size_t max) {
    size_t n=0;
    for (size_t i=0; i<regx_count && n<max; ++i) {
        if (sel) {
            if (sel->type && regx_registry[i].manifest.type != sel->type)
                continue;
            if (sel->parent_id && regx_registry[i].parent_id != sel->parent_id)
                continue;
            if (sel->name_prefix[0] &&
                strncmp(regx_registry[i].manifest.name, sel->name_prefix, strlen(sel->name_prefix)) != 0)
                continue;
        }
        out[n++] = regx_registry[i];
    }
    return n;
}

const regx_entry_t *regx_query(uint64_t id) {
    for (size_t i=0; i<regx_count; ++i)
        if (regx_registry[i].id == id)
            return &regx_registry[i];
    return NULL;
}

void regx_tree(uint64_t parent, int level) {
    for (size_t i = 0; i < regx_count; ++i) {
        if (regx_registry[i].parent_id == parent) {
            for (int l = 0; l < level; ++l) printf("  ");
            printf("%llu %s\n", (unsigned long long)regx_registry[i].id, regx_registry[i].manifest.name);
            regx_tree(regx_registry[i].id, level+1);
        }
    }
}
