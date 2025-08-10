#ifndef LIBC_H
#define LIBC_H

#include <stddef.h>
#include <stdint.h>
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#include <time.h>
/* Only declare if not found (use #ifdef or #ifndef) */
#ifndef HAVE_CLOCK_GETTIME
int clock_gettime(int clk_id, struct timespec *tp);
#endif

void serial_putc(char c);  // <-- ADD THIS

// ========== MUTEX SUPPORT ==========
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


// ===================
// FILE API
// ===================
#ifndef __FILE_defined
typedef struct FILE FILE;
#define __FILE_defined 1
#endif

// Basic file I/O functions (implemented in your libc.c)
FILE   *fopen(const char *path, const char *mode);
size_t  fread(void *ptr, size_t size, size_t nmemb, FILE *stream);
size_t  fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);
int     fclose(FILE *stream);
int     rename(const char *old, const char *new);
long    ftell(FILE *stream);
int     fseek(FILE *stream, long offset, int whence);

// ===================
// STRING / MEMORY
// ===================
void   *memset(void *s, int c, size_t n);
void   *memcpy(void *dest, const void *src, size_t n);
void   *memmove(void *dest, const void *src, size_t n);
int     memcmp(const void *s1, const void *s2, size_t n);
size_t  strlen(const char *s);
char   *strncpy(char *dest, const char *src, size_t n);
size_t  strlcpy(char *dst, const char *src, size_t size);
int     strcmp(const char *s1, const char *s2);
int     strncmp(const char *s1, const char *s2, size_t n);
char   *strchr(const char *s, int c);
char   *strcpy(char *dest, const char *src);
char   *strcat(char *dest, const char *src);
char   *strstr(const char *haystack, const char *needle);
void   *memchr(const void *s, int c, size_t n);
void   *memmem(const void *haystack, size_t haystacklen,
               const void *needle, size_t needlelen);
int     snprintf(char *str, size_t size, const char *fmt, ...);
long    strtol(const char *nptr, char **endptr, int base);

// ===================
// MEMORY ALLOCATION
// ===================
void   *malloc(size_t size);
void   *calloc(size_t nmemb, size_t size);
void    free(void *ptr);
void   *realloc(void *ptr, size_t size);

// ===================
// MATH
// ===================
int        abs(int x);
long       labs(long x);
long long  llabs(long long x);
double     sqrt(double x);

// ===================
// SYSTEM CALLS
// ===================
int   fork(void);
int   exec(const char *path);
void *sbrk(long inc);

// ===================
// SAFE MEMOPS
// ===================
void  *__memcpy_chk(void *dest, const void *src, size_t n, size_t destlen);
char  *__strncpy_chk(char *dest, const char *src, size_t n, size_t destlen);

// ===================
// TIME
// ===================
//int      clock_gettime(int clk_id, struct timespec *tp);
time_t   time(time_t *t);

#endif // LIBC_H
