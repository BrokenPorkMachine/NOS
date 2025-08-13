#pragma once
#include <stdarg.h>

// Low-level console output provided by the kernel
void kcons_putc(int c);

int putchar(int c);
int puts(const char *s);
int vprintf(const char *fmt, va_list ap);
int printf(const char *fmt, ...);
