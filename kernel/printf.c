#include <stdarg.h>
#include "drivers/IO/serial.h"

int printf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    serial_vprintf(fmt, ap);
    va_end(ap);
    return 0;
}

