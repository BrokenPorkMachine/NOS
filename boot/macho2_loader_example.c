#include <stdio.h>
#include <stdint.h>
#include "../include/macho2.h"

/* Example loader that locates the manifest and entry point */
int load_and_run(const char *path)
{
    struct macho2_manifest m;
    if (macho2_load_manifest(path, &m)) {
        fprintf(stderr, "manifest not found\n");
        return -1;
    }
    printf("agent %s (%s) entry %s\n", m.name, m.version, m.entry);
    /* In a real kernel this would map segments and jump to entry */
    /* Here we simply report the entry point symbol */
    return 0;
}

#ifdef TEST_MACHO2_LOADER
int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "usage: %s <macho2>\n", argv[0]);
        return 1;
    }
    return load_and_run(argv[1]);
}
#endif
