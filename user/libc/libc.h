#ifndef LIBC_H
#define LIBC_H

#include <stddef.h>
#include <stdint.h>
#include <time.h>

#if __has_include(<pthread.h>)
#include <pthread.h>
#else
typedef struct {
    volatile int lock;
    uint32_t owner;
    int count;
} pthread_mutex_t;
typedef void* pthread_mutexattr_t;

int pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr);
int pthread_mutex_lock(pthread_mutex_t *mutex);
int pthread_mutex_unlock(pthread_mutex_t *mutex);
int pthread_mutex_destroy(pthread_mutex_t *mutex);
#endif

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
