#include <stdarg.h>
#include "drivers/IO/serial.h"
#include "panic.h"

int printf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    serial_vprintf(fmt, ap);
    va_end(ap);
    return 0;
}

int kprintf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    serial_vprintf(fmt, ap);
    va_end(ap);
    return 0;
}

void panic(const char *fmt, ...) {
    va_list ap;
    serial_puts("[panic] ");
    va_start(ap, fmt);
    serial_vprintf(fmt, ap);
    va_end(ap);
    serial_puts("\n");
    for (;;) {
        __asm__ __volatile__("cli; hlt");
    }
}

