#ifndef LIBC_H
#define LIBC_H

#include <stddef.h>
#include <stdint.h>
#include <time.h>
#include <pthread.h>

typedef struct FILE FILE;

FILE *fopen(const char *path, const char *mode);
size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream);
size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);
int fclose(FILE *stream);

// String/mem functions
void *memset(void *s, int c, size_t n);
void *memcpy(void *dest, const void *src, size_t n);
void *memmove(void *dest, const void *src, size_t n);
int memcmp(const void *s1, const void *s2, size_t n);
size_t strlen(const char *s);
char *strncpy(char *dest, const char *src, size_t n);
size_t strlcpy(char *dst, const char *src, size_t size);
int strcmp(const char *s1, const char *s2);
int strncmp(const char *s1, const char *s2, size_t n);
char *strchr(const char *s, int c);
char *strcpy(char *dest, const char *src);
char *strcat(char *dest, const char *src);
char *strstr(const char *haystack, const char *needle);

// Math
int abs(int x);
long labs(long x);
long long llabs(long long x);
double sqrt(double x);

// System call wrappers (these are for your kernel ABI)
int fork(void);
int exec(const char *path);
void *sbrk(long inc);

// Safe checked memops
void *__memcpy_chk(void *dest, const void *src, size_t n, size_t destlen);
char *__strncpy_chk(char *dest, const char *src, size_t n, size_t destlen);

// Time
int clock_gettime(int clk_id, struct timespec *tp);
time_t time(time_t *t);

#endif // LIBC_H
