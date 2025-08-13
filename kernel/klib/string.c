// Minimal freestanding string/memory for the kernel
#include <stddef.h>
#include "string.h"

void *memcpy(void *dest, const void *src, size_t n) {
    unsigned char *d = (unsigned char*)dest;
    const unsigned char *s = (const unsigned char*)src;
    for (; n; --n) *d++ = *s++;
    return dest;
}

void *memmove(void *dest, const void *src, size_t n) {
    unsigned char *d = (unsigned char*)dest;
    const unsigned char *s = (const unsigned char*)src;
    if (d == s || n == 0) return dest;
    if (d < s) {
        for (; n; --n) *d++ = *s++;
    } else {
        d += n; s += n;
        for (; n; --n) *--d = *--s;
    }
    return dest;
}

void *memset(void *s, int c, size_t n) {
    unsigned char *p = (unsigned char*)s;
    for (; n; --n) *p++ = (unsigned char)c;
    return s;
}

int memcmp(const void *s1, const void *s2, size_t n) {
    const unsigned char *a = (const unsigned char*)s1;
    const unsigned char *b = (const unsigned char*)s2;
    for (; n; --n, ++a, ++b) {
        if (*a != *b) return (int)*a - (int)*b;
    }
    return 0;
}

size_t strlen(const char *s) {
    const char *p = s;
    while (*p) ++p;
    return (size_t)(p - s);
}

char *strcpy(char *dest, const char *src) {
    char *d = dest;
    while ((*d++ = *src++));
    return dest;
}

char *strncpy(char *dest, const char *src, size_t n) {
    char *d = dest;
    for (; n && *src; --n) *d++ = *src++;
    for (; n; --n) *d++ = 0;
    return dest;
}

int strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) { ++s1; ++s2; }
    return (unsigned char)*s1 - (unsigned char)*s2;
}

int strncmp(const char *s1, const char *s2, size_t n) {
    for (; n; --n, ++s1, ++s2) {
        if (*s1 != *s2) return (unsigned char)*s1 - (unsigned char)*s2;
        if (*s1 == 0) return 0;
    }
    return 0;
}

char *strchr(const char *s, int c) {
    char ch = (char)c;
    for (; *s; ++s) if (*s == ch) return (char*)s;
    return (ch == 0) ? (char*)s : NULL;
}

char *strrchr(const char *s, int c) {
    const char *last = NULL;
    char ch = (char)c;
    for (; *s; ++s) if (*s == ch) last = s;
    return (char*)( (ch == 0) ? s : last );
}
