#include <stdarg.h>
int kprintf(const char *fmt, ...) {
    (void)fmt;
    return 0;
}

void panic(const char *fmt, ...) {
    (void)fmt;
}
