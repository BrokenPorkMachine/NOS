#include <stdint.h>
#include "io.h"
#include "serial.h"

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
