#pragma once
#include <stdint.h>
#include <stdarg.h>
#include <stddef.h>

void serial_init(void);
void serial_write(char c);
void serial_puts(const char *s);
void serial_putsn(const char *s, size_t n);
void serial_puthex(uint32_t value);
void serial_vprintf(const char *fmt, va_list ap);
void serial_printf(const char *fmt, ...);
int serial_read(void);
