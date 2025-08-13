#ifndef NOS_STRING_H
#define NOS_STRING_H

#include <stddef.h>

#ifdef KERNEL_BUILD
void   *memset(void *s, int c, size_t n);
void   *memcpy(void *dest, const void *src, size_t n);
void   *memmove(void *dest, const void *src, size_t n);
int     memcmp(const void *s1, const void *s2, size_t n);
void   *memchr(const void *s, int c, size_t n);
void   *memmem(const void *haystack, size_t haystacklen,
               const void *needle, size_t needlelen);

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
#else
#include_next <string.h>
#endif

#endif // NOS_STRING_H
