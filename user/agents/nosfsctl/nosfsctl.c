// bin/nosfsctl.c  — simple “nosfs control” user tool using project libc only.
#include "../user/libc/libc.h"   // your freestanding stdio/stdio-like API
#include <stdint.h>
#include <string.h>

static void usage(const char *argv0) {
    printf("usage: %s <list|cat> [path]\n", argv0);
}

int main(int argc, char **argv) {
    if (argc < 2) { usage(argv[0]); return 1; }

    if (!strcmp(argv[1], "list")) {
        const char *path = (argc >= 3) ? argv[2] : "/";
        // very basic poor-man’s “ls”: read a text index file exported by NOSFS
        // (adjust to whatever your NOSFS exposes; this is just a demo).
        char idxpath[256];
        snprintf(idxpath, sizeof(idxpath), "%s/.index", path);

        FILE *f = fopen(idxpath, "r");
        if (!f) { printf("nosfsctl: cannot open %s\n", idxpath); return 1; }

        char line[256];
        while (fgets(line, sizeof(line), f)) {
            // strip newline
            size_t n = strlen(line);
            if (n && (line[n-1] == '\n' || line[n-1] == '\r')) line[n-1] = 0;
            printf("%s\n", line);
        }
        fclose(f);
        return 0;
    }

    if (!strcmp(argv[1], "cat")) {
        if (argc < 3) { usage(argv[0]); return 1; }
        const char *path = argv[2];
        FILE *f = fopen(path, "r");
        if (!f) { printf("nosfsctl: cannot open %s\n", path); return 1; }

        char buf[1024];
        size_t n;
        while ((n = fread(buf, 1, sizeof(buf), f)) > 0)
            fwrite(buf, 1, n, stdout);
        fclose(f);
        return 0;
    }

    usage(argv[0]);
    return 1;
}
