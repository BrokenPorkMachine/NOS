#include <stdarg.h>
int kprintf(const char *fmt, ...) {
    (void)fmt;
    return 0;
}

__attribute__((weak)) void panic(const char *fmt, ...) {
    (void)fmt;
}
