#include <stdarg.h>
#include "drivers/IO/serial.h"
#include "klib/stdio.h"
#include "panic.h"

/*
 * Provide a low-level console character output so the minimal stdio
 * implementation in klib can route all formatted printing through the
 * serial driver.  This replaces the old standalone printf implementation
 * and avoids duplicate symbol definitions when linking.
 */
void kcons_putc(int c) {
    if (c == '\n')
        serial_write('\r');
    serial_write((char)c);
}

int kprintf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int ret = vprintf(fmt, ap);
    va_end(ap);
    return ret;
}

void panic(const char *fmt, ...) {
    va_list ap;
    serial_puts("[panic] ");
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    serial_puts("\n");
    for (;;) {
        __asm__ __volatile__("cli; hlt");
    }
}

