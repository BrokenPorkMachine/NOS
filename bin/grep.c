#include "../user/libc/libc.h"

// Simple grep-like tool: returns 0 if pattern is found in file "src".
int main(void) {
    const char *pattern = "foo";
    FILE *f = fopen("src", "r");
    if (!f) return 1;

    char buf[512];
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    buf[n] = '\0';
    fclose(f);

    return strstr(buf, pattern) ? 0 : 1;
}

