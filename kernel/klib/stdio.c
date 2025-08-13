// Minimal freestanding stdio for the kernel
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include "stdio.h"
#include <stdint.h>

// If your kernel already has a low-level console putc, declare it here.
// Provide one of these somewhere in the kernel (serial, VGA, etc.):
//   void kcons_putc(int c);
// If you don't have it yet, these fallbacks just discard output.
__attribute__((weak)) void kcons_putc(int c) { (void)c; }

// ---- putchar / puts ----
int putchar(int c) {
    kcons_putc((unsigned char)c);
    return (unsigned char)c;
}

int puts(const char *s) {
    if (!s) return 0;
    while (*s) putchar(*s++);
    putchar('\n');
    return 0;
}

// ---- tiny helpers for printf ----
static void k_puts_dec(long long v) {
    char buf[32];
    int i = 0;
    unsigned long long n;

    if (v < 0) { putchar('-'); n = (unsigned long long)(-v); }
    else       { n = (unsigned long long)( v); }

    if (n == 0) { putchar('0'); return; }
    while (n && i < (int)sizeof(buf)) { buf[i++] = '0' + (n % 10); n /= 10; }
    while (i--) putchar(buf[i]);
}

static void k_puts_udec(unsigned long long n) {
    char buf[32];
    int i = 0;
    if (n == 0) { putchar('0'); return; }
    while (n && i < (int)sizeof(buf)) { buf[i++] = '0' + (n % 10); n /= 10; }
    while (i--) putchar(buf[i]);
}

static void k_puts_hex(unsigned long long n, int prefix) {
    const char *hex = "0123456789abcdef";
    char buf[32];
    int i = 0;
    if (prefix) { putchar('0'); putchar('x'); }
    if (n == 0) { putchar('0'); return; }
    while (n && i < (int)sizeof(buf)) { buf[i++] = hex[n & 0xF]; n >>= 4; }
    while (i--) putchar(buf[i]);
}

// ---- vprintf / printf (minimal: %s %c %d %u %x %p %% ) ----
int vprintf(const char *fmt, va_list ap) {
    int out = 0;
    if (!fmt) return 0;

    for (; *fmt; ++fmt) {
        if (*fmt != '%') { putchar(*fmt); ++out; continue; }
        ++fmt;
        if (!*fmt) break;

        switch (*fmt) {
            case '%': putchar('%'); ++out; break;
            case 'c': { int c = va_arg(ap, int); putchar(c); ++out; } break;
            case 's': {
                const char *s = va_arg(ap, const char*);
                if (!s) s = "(null)";
                while (*s) { putchar(*s++); ++out; }
            } break;
            case 'd': case 'i': {
                long long v = va_arg(ap, long long);
                k_puts_dec(v);
                // not tracking exact count for speed (optional)
            } break;
            case 'u': {
                unsigned long long v = va_arg(ap, unsigned long long);
                k_puts_udec(v);
            } break;
            case 'x': {
                unsigned long long v = va_arg(ap, unsigned long long);
                k_puts_hex(v, 0);
            } break;
            case 'p': {
                unsigned long long v = (unsigned long long)(uintptr_t)va_arg(ap, void*);
                k_puts_hex(v, 1);
            } break;
            default:
                // Unknown specifier, print literally to avoid surprises
                putchar('%'); putchar(*fmt);
                out += 2;
                break;
        }
    }
    return out;
}

int printf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int r = vprintf(fmt, ap);
    va_end(ap);
    return r;
}
