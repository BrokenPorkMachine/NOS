#pragma once

// Minimal C string and memory interface for freestanding builds.  The
// kernel and agents supply their own implementations in user/libc/libc.c
// so we cannot rely on the host's <string.h> being available.  Provide the
// commonly used prototypes here instead of using `#include_next`.

#include <stddef.h>

/* memory routines */
void   *memset(void *s, int c, size_t n);
void   *memcpy(void *dest, const void *src, size_t n);
void   *memmove(void *dest, const void *src, size_t n);
int     memcmp(const void *s1, const void *s2, size_t n);
void   *memchr(const void *s, int c, size_t n);
void   *memmem(const void *haystack, size_t haystacklen,
               const void *needle, size_t needlelen);

/* string routines */
size_t  strlen(const char *s);
size_t  strnlen(const char *s, size_t max);
char   *strcpy(char *dest, const char *src);
char   *strncpy(char *dest, const char *src, size_t n);
size_t  strlcpy(char *dst, const char *src, size_t size);
char   *strcat(char *dest, const char *src);
int     strcmp(const char *s1, const char *s2);
int     strncmp(const char *s1, const char *s2, size_t n);
char   *strchr(const char *s, int c);
char   *strrchr(const char *s, int c);
char   *strstr(const char *haystack, const char *needle);

/* misc */
int     snprintf(char *str, size_t size, const char *fmt, ...);
long    strtol(const char *nptr, char **endptr, int base);

