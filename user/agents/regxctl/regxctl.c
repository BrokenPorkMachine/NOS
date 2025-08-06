#include "regx.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

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
            printf("%llu %s %s gen=%u state=%d\n",
                   (unsigned long long)entries[i].id,
                   entries[i].manifest.name,
                   entries[i].manifest.version,
                   entries[i].generation,
                   entries[i].state);
    } else if (strcmp(argv[1], "query") == 0 && argc >= 3) {
        uint64_t id = strtoull(argv[2], NULL, 0);
        const regx_entry_t *e = regx_query(id);
        if (e)
            printf("%llu %s %s gen=%u state=%d\n",
                   (unsigned long long)e->id,
                   e->manifest.name,
                   e->manifest.version,
                   e->generation,
                   e->state);
    } else if (strcmp(argv[1], "manifest") == 0 && argc >= 3) {
        uint64_t id = strtoull(argv[2], NULL, 0);
        const regx_entry_t *e = regx_query(id);
        if (e)
            printf("id=%llu\nname=%s\ntype=%d\nversion=%s\nabi=%s\ncapabilities=%s\nstate=%d\ngeneration=%u\n",
                   (unsigned long long)e->id,
                   e->manifest.name,
                   e->manifest.type,
                   e->manifest.version,
                   e->manifest.abi,
                   e->manifest.capabilities,
                   e->state,
                   e->generation);
    } else if (strcmp(argv[1], "tree") == 0) {
        regx_tree(0, 0);
    } else {
        usage();
        return 1;
    }
    return 0;
}
