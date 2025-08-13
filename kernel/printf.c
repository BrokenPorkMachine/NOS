#include <stdarg.h>
#include "drivers/IO/serial.h"
#include "panic.h"

static int kvprintf(const char *fmt, va_list ap) {
    serial_vprintf(fmt, ap);
    return 0;
}

int printf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int ret = kvprintf(fmt, ap);
    va_end(ap);
    return ret;
}

int kprintf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int ret = kvprintf(fmt, ap);
    va_end(ap);
    return ret;
}

void panic(const char *fmt, ...) {
    va_list ap;
    serial_puts("[panic] ");
    va_start(ap, fmt);
    kvprintf(fmt, ap);
    va_end(ap);
    serial_puts("\n");
    for (;;) {
        __asm__ __volatile__("cli; hlt");
    }
}

