#include "regx.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* Stub: In real OS this would be syscalls or IPC */
extern size_t regx_enumerate(const regx_selector_t *sel, regx_entry_t *out, size_t max);
extern const regx_entry_t *regx_query(uint64_t id);
extern void regx_tree(uint64_t parent, int level);

static void usage(void) {
    printf("regxctl list\n");
    printf("regxctl query <id>\n");
    printf("regxctl manifest <id>\n");
    printf("regxctl tree\n");
}

int main(int argc, char **argv) {
    if (argc < 2) {
        usage();
        return 1;
    }
    if (strcmp(argv[1], "list") == 0) {
        regx_entry_t entries[64];
        size_t n = regx_enumerate(NULL, entries, 64);
        for (size_t i = 0; i < n; ++i)
            printf("%llu %s %s\n", (unsigned long long)entries[i].id,
                entries[i].manifest.name, entries[i].manifest.version);
    } else if (strcmp(argv[1], "query") == 0 && argc >= 3) {
        uint64_t id = strtoull(argv[2], NULL, 0);
        const regx_entry_t *e = regx_query(id);
        if (e)
            printf("%llu %s %s\n", (unsigned long long)e->id,
                e->manifest.name, e->manifest.version);
    } else if (strcmp(argv[1], "manifest") == 0 && argc >= 3) {
        uint64_t id = strtoull(argv[2], NULL, 0);
        const regx_entry_t *e = regx_query(id);
        if (e)
            printf("name=%s\ntype=%d\nversion=%s\nabi=%s\ncapabilities=%s\n",
                e->manifest.name, e->manifest.type, e->manifest.version,
                e->manifest.abi, e->manifest.capabilities);
    } else if (strcmp(argv[1], "tree") == 0) {
        regx_tree(0, 0);
    } else {
        usage();
        return 1;
    }
    return 0;
}
