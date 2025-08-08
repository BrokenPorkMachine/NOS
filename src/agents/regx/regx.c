#include <regx.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>

__attribute__((section("__O2INFO,__manifest")))
const char mo2_manifest[] =
"{\n"
"  \"name\": \"regx\",\n"
"  \"type\": \"service\",\n"
"  \"version\": \"1.0.0\",\n"
"  \"abi\": \"regx-v1\",\n"
"  \"entry\": \"regx_main\"\n"
"}\n";

// --- Registry State ---
static regx_entry_t regx_registry[REGX_MAX_ENTRIES];
static size_t regx_count = 0;
static uint64_t regx_next_id = 1;

// --- Registration/Unregistration ---
uint64_t regx_register(const regx_manifest_t *m, uint64_t parent_id) {
    if (!m || regx_count >= REGX_MAX_ENTRIES)
        return 0;

    // Prevent duplicate name under same parent
    for (size_t i = 0; i < regx_count; ++i) {
        if (regx_registry[i].parent_id == parent_id &&
            strncmp(regx_registry[i].manifest.name, m->name, sizeof(m->name)) == 0)
            return 0; // Already registered
    }

    regx_entry_t *e = &regx_registry[regx_count++];
    e->id = regx_next_id++;
    e->parent_id = parent_id;
    e->manifest = *m;
    return e->id;
}

int regx_unregister(uint64_t id) {
    for (size_t i=0; i<regx_count; ++i) {
        if (regx_registry[i].id == id) {
            // Remove children recursively (optional: cascade delete)
            for (size_t j=0; j<regx_count; ) {
                if (regx_registry[j].parent_id == id) {
                    regx_unregister(regx_registry[j].id);
                    continue; // Do not increment
                }
                ++j;
            }
            // Overwrite with last and reduce count
            regx_registry[i] = regx_registry[--regx_count];
            return 0;
        }
    }
    return -1;
}

// --- Enumeration with strong bounds and prefix support ---
size_t regx_enumerate(const regx_selector_t *sel, regx_entry_t *out, size_t max) {
    if (!out || max == 0) return 0;
    size_t n=0;
    for (size_t i=0; i<regx_count && n<max; ++i) {
        if (sel) {
            if (sel->type && regx_registry[i].manifest.type != sel->type)
                continue;
            if (sel->parent_id && regx_registry[i].parent_id != sel->parent_id)
                continue;
            if (sel->name_prefix[0]) {
                size_t prefix_len = strnlen(sel->name_prefix, sizeof(sel->name_prefix));
                if (strncmp(regx_registry[i].manifest.name, sel->name_prefix, prefix_len) != 0)
                    continue;
            }
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

// --- Tree Dump (for tools/diagnostics) ---
void regx_tree(uint64_t parent, int level, FILE *outf) {
    for (size_t i = 0; i < regx_count; ++i) {
        if (regx_registry[i].parent_id == parent) {
            for (int l = 0; l < level; ++l) fprintf(outf, "  ");
            fprintf(outf, "%llu %s\n", (unsigned long long)regx_registry[i].id, regx_registry[i].manifest.name);
            regx_tree(regx_registry[i].id, level+1, outf);
        }
    }
}

// --- JSON Export of Registry ---
void regx_export_json(FILE *outf) {
    fprintf(outf, "[\n");
    for (size_t i=0; i<regx_count; ++i) {
        const regx_entry_t *e = &regx_registry[i];
        fprintf(outf, "  { \"id\": %llu, \"parent\": %llu, \"name\": \"%s\", \"type\": %d, "
                "\"version\": \"%s\", \"abi\": \"%s\", \"capabilities\": \"%s\" }%s\n",
                (unsigned long long)e->id, (unsigned long long)e->parent_id,
                e->manifest.name, e->manifest.type,
                e->manifest.version, e->manifest.abi, e->manifest.capabilities,
                (i+1==regx_count)?"":",");
    }
    fprintf(outf, "]\n");
}
