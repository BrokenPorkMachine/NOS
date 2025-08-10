#include <stdint.h>
#include "io.h"
#include "serial.h"
#include <stdarg.h>

#define COM1 0x3F8

void serial_init(void) {
    outb(COM1 + 1, 0x00);    // Disable all interrupts
    outb(COM1 + 3, 0x80);    // Enable DLAB
    outb(COM1 + 0, 0x03);    // Set divisor to 3 (38400 baud) (low byte)
    outb(COM1 + 1, 0x00);    // High byte
    outb(COM1 + 3, 0x03);    // 8N1
    outb(COM1 + 2, 0xC7);    // Enable FIFO, clear, 14-byte threshold
    outb(COM1 + 4, 0x0B);    // IRQs enabled, RTS/DSR set
}

static int serial_received(void) {
    return inb(COM1 + 5) & 0x01;
}

static int serial_empty(void) {
    return inb(COM1 + 5) & 0x20;
}

void serial_write(char c) {
    while (!serial_empty()) {
        ;
    }
    outb(COM1, c);
}

void serial_puts(const char *s) {
    while (*s) {
        if (*s == '\n')
            serial_write('\r');
        serial_write(*s++);
    }
}

void serial_puthex(uint32_t value) {
    char buf[9];
    for (int i = 7; i >= 0; --i) {
        buf[i] = "0123456789ABCDEF"[value & 0xF];
        value >>= 4;
    }
    buf[8] = '\0';

    char *p = buf;
    while (*p == '0' && *(p + 1))
        p++;
    serial_puts(p);
}

static void serial_print_uint(uint64_t value, int base, int width, int pad_zero) {
    char buf[32];
    int i = 0;
    const char *digits = "0123456789abcdef";

    if (value == 0) {
        buf[i++] = '0';
    } else {
        while (value > 0) {
            buf[i++] = digits[value % base];
            value /= base;
        }
    }

    while (i < width)
        buf[i++] = pad_zero ? '0' : ' ';

    for (int j = i - 1; j >= 0; --j)
        serial_write(buf[j]);
}

void serial_vprintf(const char *fmt, va_list ap) {
    for (const char *p = fmt; *p; ++p) {
        if (*p != '%') {
            if (*p == '\n')
                serial_write('\r');
            serial_write(*p);
            continue;
        }

        int pad_zero = 0;
        int width = 0;

        ++p;
        if (*p == '0') {
            pad_zero = 1;
            ++p;
        }
        while (*p >= '0' && *p <= '9') {
            width = width * 10 + (*p - '0');
            ++p;
        }

        int long_flag = 0;
        if (*p == 'l' || *p == 'z') {
            long_flag = 1;
            ++p;
        }

        switch (*p) {
        case 'x': {
            uint64_t v = long_flag ? va_arg(ap, uint64_t) : va_arg(ap, unsigned int);
            serial_print_uint(v, 16, width, pad_zero);
            break;
        }
        case 'u': {
            uint64_t v = long_flag ? va_arg(ap, uint64_t) : va_arg(ap, unsigned int);
            serial_print_uint(v, 10, width, pad_zero);
            break;
        }
        case 'd': {
            int64_t v = long_flag ? va_arg(ap, int64_t) : va_arg(ap, int);
            if (v < 0) {
                serial_write('-');
                v = -v;
            }
            serial_print_uint((uint64_t)v, 10, width, pad_zero);
            break;
        }
        case 'c': {
            char c = (char)va_arg(ap, int);
            serial_write(c);
            break;
        }
        case 's': {
            const char *s = va_arg(ap, const char *);
            serial_puts(s);
            break;
        }
        case 'p': {
            uintptr_t v = (uintptr_t)va_arg(ap, void *);
            serial_write('0');
            serial_write('x');
            serial_print_uint(v, 16, sizeof(void*) * 2, 1);
            break;
        }
        case '%':
            serial_write('%');
            break;
        default:
            serial_write('%');
            serial_write(*p);
            break;
        }
    }
}

void serial_printf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    serial_vprintf(fmt, ap);
    va_end(ap);
}

int serial_read(void) {
    if (!serial_received())
        return -1;
    return inb(COM1);
}
