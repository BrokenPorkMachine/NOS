#include "../user/libc/libc.h"

// Minimal copy implementation using project libc only.
int main(void) {
    FILE *src = fopen("src", "r");
    if (!src) return 1;

    FILE *dst = fopen("dst", "w");
    if (!dst) {
        fclose(src);
        return 1;
    }

    unsigned char buf[512];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), src)) > 0) {
        fwrite(buf, 1, n, dst);
    }

    fclose(src);
    fclose(dst);
    return 0;
}

