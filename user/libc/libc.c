#include "libc.h"
#include "printf.h"
#include <stdint.h>
#include <stddef.h>
#include <time.h>
#include <stdarg.h>

// -------- String safety guards (prevent #PF on bad pointers) --------
static inline int __nitros_is_canonical_ptr(const void *p) {
    uintptr_t x = (uintptr_t)p;
    return ((x >> 48) == 0) || ((x >> 48) == 0xFFFF);
}
// limit any single string walk; 1MB is generous and prevents runaways
#ifndef NITROS_STR_MAX
#define NITROS_STR_MAX (1u<<20)
#endif
static inline size_t __nitros_safe_strnlen(const char *s, size_t max) {
    if (!s || !__nitros_is_canonical_ptr(s)) return 0;
    size_t n = 0; while (n < max) { char c = s[n]; if (!c) break; n++; } return n;
}

// ---- Your kernel-mode recursive spinlock mutex ----
extern uint32_t thread_self(void);
// Provide a weak default implementation for unit tests
__attribute__((weak)) uint32_t thread_self(void) { return 1; }

int pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr) {
    (void)attr;
    mutex->lock = 0;
    mutex->owner = (uint32_t)-1;
    mutex->count = 0;
    return 0;
}
int pthread_mutex_lock(pthread_mutex_t *mutex) {
    uint32_t self = thread_self();
    if (mutex->owner == self) {
        mutex->count++;
        return 0;
    }
    while (__sync_lock_test_and_set(&mutex->lock, 1));
    mutex->owner = self;
    mutex->count = 1;
    return 0;
}
int pthread_mutex_unlock(pthread_mutex_t *mutex) {
    if (mutex->owner != thread_self())
        return -1;
    if (--mutex->count == 0) {
        mutex->owner = (uint32_t)-1;
        __sync_lock_release(&mutex->lock);
    }
    return 0;
}
int pthread_mutex_destroy(pthread_mutex_t *mutex) {
    (void)mutex;
    return 0;
}

// ================== STRING AND MEMORY ===================
void *memset(void *s, int c, size_t n) {
    unsigned char *p = (unsigned char*)s;
    while (n--) *p++ = (unsigned char)c;
    return s;
}

void *memcpy(void *dest, const void *src, size_t n) {
    unsigned char *d = (unsigned char*)dest;
    const unsigned char *s = (const unsigned char*)src;
    while (n--) *d++ = *s++;
    return dest;
}

void *memmove(void *dest, const void *src, size_t n) {
    unsigned char *d = (unsigned char*)dest;
    const unsigned char *s = (const unsigned char*)src;
    if (d < s) {
        while (n--) *d++ = *s++;
    } else if (d > s) {
        d += n;
        s += n;
        while (n--) *(--d) = *(--s);
    }
    return dest;
}

int memcmp(const void *s1, const void *s2, size_t n) {
    const unsigned char *p1 = (const unsigned char*)s1;
    const unsigned char *p2 = (const unsigned char*)s2;
    for (size_t i = 0; i < n; i++) {
        if (p1[i] != p2[i]) return p1[i] - p2[i];
    }
    return 0;
}

size_t strlen(const char *s) {
    return __nitros_safe_strnlen(s, NITROS_STR_MAX);
}

size_t strnlen(const char *s, size_t max) {
    if (max > NITROS_STR_MAX) max = NITROS_STR_MAX;
    return __nitros_safe_strnlen(s, max);
}

char *strncpy(char *dest, const char *src, size_t n) {
    size_t i;
    for (i = 0; i < n && src[i]; ++i) dest[i] = src[i];
    for (; i < n; ++i) dest[i] = '\0';
    return dest;
}

size_t strlcpy(char *dst, const char *src, size_t size) {
    size_t srclen = __nitros_safe_strnlen(src, NITROS_STR_MAX);
    if (!__nitros_is_canonical_ptr(dst) || size == 0) return srclen;
    if (size) {
        size_t copylen = (srclen >= size) ? size - 1 : srclen;
        memcpy(dst, src, copylen);
        dst[copylen] = '\0';
    }
    return srclen;
}

int strcmp(const char *s1, const char *s2) {
    size_t n1 = __nitros_safe_strnlen(s1, NITROS_STR_MAX);
    size_t n2 = __nitros_safe_strnlen(s2, NITROS_STR_MAX);
    size_t n  = (n1 < n2 ? n1 : n2);
    for (size_t i = 0; i < n; ++i) {
        unsigned char a = (unsigned char)s1[i];
        unsigned char b = (unsigned char)s2[i];
        if (a != b) return (int)a - (int)b;
    }
    if (n1 == n2) return 0;
    return (n1 < n2) ? -1 : 1;
}

int strncmp(const char *s1, const char *s2, size_t n) {
    if (!n) return 0;
    size_t cap = (n > NITROS_STR_MAX) ? NITROS_STR_MAX : n;
    size_t n1 = __nitros_safe_strnlen(s1, cap);
    size_t n2 = __nitros_safe_strnlen(s2, cap);
    size_t m  = (n1 < n2 ? n1 : n2);
    for (size_t i = 0; i < m; ++i) {
        unsigned char a = (unsigned char)s1[i];
        unsigned char b = (unsigned char)s2[i];
        if (a != b) return (int)a - (int)b;
    }
    if (m == cap) return 0;        // equal up to cap
    if (n1 == n2) return 0;        // both ended
    return (n1 < n2) ? -1 : 1;
}

void *memchr(const void *s, int c, size_t n) {
    const unsigned char *p = (const unsigned char *)s;
    for (size_t i = 0; i < n; ++i) {
        if (p[i] == (unsigned char)c)
            return (void *)(p + i);
    }
    return NULL;
}

void *memmem(const void *haystack, size_t haystacklen,
             const void *needle, size_t needlelen) {
    if (needlelen == 0) return (void *)haystack;
    const unsigned char *h = (const unsigned char *)haystack;
    const unsigned char *n = (const unsigned char *)needle;
    for (size_t i = 0; i + needlelen <= haystacklen; ++i) {
        if (h[i] == n[0] && memcmp(h + i, n, needlelen) == 0)
            return (void *)(h + i);
    }
    return NULL;
}

static int itoa_dec(char *buf, size_t buf_sz, int val) {
    char tmp[32];
    int neg = val < 0;
    unsigned int u = neg ? -val : val;
    int i = 0;
    do { tmp[i++] = '0' + (u % 10); u /= 10; } while (u && i < (int)sizeof(tmp));
    if (neg && i < (int)sizeof(tmp)) tmp[i++] = '-';
    if (i >= (int)buf_sz) i = buf_sz - 1;
    for (int j = 0; j < i; ++j) buf[j] = tmp[i - j - 1];
    buf[i] = '\0';
    return i;
}

int snprintf(char *str, size_t size, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    size_t pos = 0;
    for (const char *p = fmt; *p && pos + 1 < size; ++p) {
        if (*p != '%') {
            str[pos++] = *p;
            continue;
        }
        ++p;
        if (*p == 's') {
            const char *s = va_arg(ap, const char *);
            size_t slen = __nitros_safe_strnlen(s, NITROS_STR_MAX);
            while (slen-- && pos + 1 < size) { str[pos++] = *s++; }
        } else if (*p == 'd') {
            char tmp[32];
            itoa_dec(tmp, sizeof(tmp), va_arg(ap, int));
            for (char *t = tmp; *t && pos + 1 < size; ++t) str[pos++] = *t;
        } else {
            str[pos++] = '%';
            if (pos + 1 < size) str[pos++] = *p;
        }
    }
    str[pos] = '\0';
    va_end(ap);
    return (int)pos;
}

long strtol(const char *nptr, char **endptr, int base) {
    (void)base;
    const char *p = nptr;
    while (*p == ' ' || *p == '\t') p++;
    int neg = 0;
    if (*p == '+' || *p == '-') {
        neg = (*p == '-');
        p++;
    }
    long val = 0;
    while (*p >= '0' && *p <= '9') {
        val = val * 10 + (*p - '0');
        p++;
    }
    if (endptr) *endptr = (char *)p;
    return neg ? -val : val;
}

char *strchr(const char *s, int c) {
    while (*s) {
        if (*s == (char)c)
            return (char *)s;
        s++;
    }
    return NULL;
}

char *strcpy(char *dest, const char *src) {
    char *d = dest;
    while ((*d++ = *src++));
    return dest;
}

char *strcat(char *dest, const char *src) {
    char *d = dest + strlen(dest);
    while ((*d++ = *src++));
    return dest;
}

char *strstr(const char *haystack, const char *needle) {
    if (!*needle) return (char *)haystack;
    size_t nlen = strlen(needle);
    for (; *haystack; haystack++)
        if (*haystack == *needle && strncmp(haystack, needle, nlen) == 0)
            return (char *)haystack;
    return NULL;
}

// ================== MATH ===================
int abs(int x) { return x < 0 ? -x : x; }
long labs(long x) { return x < 0 ? -x : x; }
long long llabs(long long x) { return x < 0 ? -x : x; }
double sqrt(double x) {
    if (x <= 0) return 0;
    double r = x;
    for (int i = 0; i < 20; ++i) r = 0.5 * (r + x / r);
    return r;
}

// ================== SYSTEM CALLS ===============
#define SYS_FORK 2
#define SYS_EXEC 3
#define SYS_SBRK 4
#define SYS_CLOCK_GETTIME 7
#define SYS_OPEN 8
#define SYS_READ 9
#define SYS_WRITE 10
#define SYS_CLOSE 11
#define SYS_LSEEK 12
#define SYS_RENAME 13

static inline long syscall3(long n, long a1, long a2, long a3) {
    long ret;
    asm volatile("mov %1, %%rax; mov %2, %%rdi; mov %3, %%rsi; mov %4, %%rdx; int $0x80; mov %%rax, %0"
                 : "=r"(ret)
                 : "r"(n), "r"(a1), "r"(a2), "r"(a3)
                 : "rax", "rdi", "rsi", "rdx");
    return ret;
}

int fork(void) { return (int)syscall3(SYS_FORK, 0, 0, 0); }
int exec(const char *path) { return (int)syscall3(SYS_EXEC, (long)path, 0, 0); }
void *sbrk(long inc) { return (void *)syscall3(SYS_SBRK, inc, 0, 0); }

// ================== THREADING: RECURSIVE MUTEX ===================

// ================== MALLOC FAMILY: THREAD-SAFE ===================
#define HEAP_SIZE (1024 * 1024)
#define HEAP_MAGIC 0xC0DECAFE

typedef struct block_header {
    size_t size;
    int    free;
    struct block_header *next;
    uint32_t magic;
} block_header_t;

static uint8_t __attribute__((aligned(16))) heap[HEAP_SIZE];
static block_header_t *free_list = NULL;
static pthread_mutex_t malloc_lock = {0};

static void heap_init(void) {
    free_list = (block_header_t *)heap;
    free_list->size  = HEAP_SIZE - sizeof(block_header_t);
    free_list->free  = 1;
    free_list->next  = NULL;
    free_list->magic = HEAP_MAGIC;
}

static block_header_t *find_block(size_t size) {
    block_header_t *cur = free_list;
    while (cur) {
        if (cur->magic != HEAP_MAGIC)
            return NULL;
        if (cur->free && cur->size >= size)
            return cur;
        cur = cur->next;
    }
    return NULL;
}

static void split_block(block_header_t *block, size_t size) {
    if (block->size <= size + sizeof(block_header_t))
        return;
    block_header_t *new_block = (block_header_t *)((uint8_t *)block +
        sizeof(block_header_t) + size);
    new_block->size  = block->size - size - sizeof(block_header_t);
    new_block->free  = 1;
    new_block->next  = block->next;
    new_block->magic = HEAP_MAGIC;
    block->size = size;
    block->next = new_block;
}

void *malloc(size_t size) {
    if (!size) return NULL;
    pthread_mutex_lock(&malloc_lock);
    if (!free_list)
        heap_init();
    size = (size + 7) & ~((size_t)7);
    block_header_t *block = find_block(size);
    if (!block) {
        pthread_mutex_unlock(&malloc_lock);
        return NULL;
    }
    split_block(block, size);
    block->free = 0;
    pthread_mutex_unlock(&malloc_lock);
    return (uint8_t *)block + sizeof(block_header_t);
}

void *calloc(size_t nmemb, size_t size) {
    if (size && nmemb > SIZE_MAX / size)
        return NULL;
    size_t total = nmemb * size;
    void *p = malloc(total);
    if (p) memset(p, 0, total);
    return p;
}

void free(void *ptr) {
    if (!ptr) return;
    pthread_mutex_lock(&malloc_lock);
    block_header_t *block = (block_header_t *)((uint8_t *)ptr - sizeof(block_header_t));
    if (block->magic != HEAP_MAGIC || block->free) {
        pthread_mutex_unlock(&malloc_lock);
        return;
    }
    memset(ptr, 0, block->size);
    block->free = 1;
    // Merge adjacent free blocks
    block_header_t *cur = free_list;
    while (cur && cur->next) {
        if (cur->free && cur->next->free &&
            ((uint8_t *)cur + sizeof(block_header_t) + cur->size == (uint8_t *)cur->next)) {
            cur->size += sizeof(block_header_t) + cur->next->size;
            cur->next = cur->next->next;
            continue;
        }
        cur = cur->next;
    }
    pthread_mutex_unlock(&malloc_lock);
}

void *realloc(void *ptr, size_t size) {
    if (!ptr) return malloc(size);
    if (size == 0) {
        free(ptr);
        return NULL;
    }
    pthread_mutex_lock(&malloc_lock);
    block_header_t *block = (block_header_t *)((uint8_t *)ptr - sizeof(block_header_t));
    size_t copy_size = (block->size < size) ? block->size : size;
    pthread_mutex_unlock(&malloc_lock);
    void *new_ptr = malloc(size);
    if (!new_ptr) return NULL;
    memcpy(new_ptr, ptr, copy_size);
    free(ptr);
    return new_ptr;
}

// ================== SAFE CHECKED MEMORY OPS =============
void *__memcpy_chk(void *dest, const void *src, size_t n, size_t destlen) {
    if (n > destlen) n = destlen;
    return memcpy(dest, src, n);
}

char *__strncpy_chk(char *dest, const char *src, size_t n, size_t destlen) {
    if (destlen == 0) return dest;
    if (n >= destlen) n = destlen - 1;
    strncpy(dest, src, n);
    dest[n] = '\0';
    return dest;
}

// ================== TIME SUPPORT =========================
/*
int clock_gettime(int clk_id, struct timespec *tp) {
    if (!tp) return -1;
    long ret;
    // Kernel syscall: cast all args to long (pointers!) for x86_64
    asm volatile("mov %1, %%rax; mov %2, %%rdi; mov %3, %%rsi; int $0x80; mov %%rax, %0"
        : "=r"(ret)
        : "r"((long)SYS_CLOCK_GETTIME), "r"((long)clk_id), "r"((long)(uintptr_t)tp)
        : "rax", "rdi", "rsi");
    if (ret == 0) return 0;
#if defined(__x86_64__)
    uint64_t tsc_lo, tsc_hi;
    __asm__ volatile ("rdtsc" : "=a"(tsc_lo), "=d"(tsc_hi));
    uint64_t tsc = ((uint64_t)tsc_hi << 32) | tsc_lo;
    uint64_t freq = 3000000000ULL; // 3 GHz default
    tp->tv_sec = tsc / freq;
    tp->tv_nsec = ((tsc % freq) * 1000000000ULL) / freq;
#else
    static uint64_t fake_ticks = 0;
    fake_ticks += 10000000;
    tp->tv_sec = fake_ticks / 1000000000ULL;
    tp->tv_nsec = fake_ticks % 1000000000ULL;
#endif
    return 0;
}

time_t time(time_t *t) {
    struct timespec ts = {0, 0};
    clock_gettime(0, &ts);
    if (t) *t = ts.tv_sec;
    return ts.tv_sec;
}
*/
// ================== FILE I/O (NOSFS) ===================
// Minimal syscall-backed FILE I/O implementation.

typedef struct FILE {
    int fd;
} FILE;

#define O_RDONLY 0
#define O_WRONLY 1
#define O_RDWR   2
#define O_CREAT  64
#define O_TRUNC  512
#define O_APPEND 1024

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

FILE *fopen(const char *path, const char *mode) {
    int flags = 0;
    if (mode && mode[0] == 'r') {
        flags = O_RDONLY;
    } else if (mode && mode[0] == 'w') {
        flags = O_WRONLY | O_CREAT | O_TRUNC;
    } else if (mode && mode[0] == 'a') {
        flags = O_WRONLY | O_CREAT | O_APPEND;
    } else {
        return NULL;
    }

    long fd = syscall3(SYS_OPEN, (long)path, flags, 0644);
    if (fd < 0)
        return NULL;

    FILE *f = (FILE *)malloc(sizeof(FILE));
    if (!f) {
        syscall3(SYS_CLOSE, fd, 0, 0);
        return NULL;
    }
    f->fd = (int)fd;
    return f;
}

size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream) {
    size_t total = size * nmemb;
    long ret = syscall3(SYS_READ, stream->fd, (long)ptr, total);
    if (ret < 0)
        return 0;
    return (size_t)ret / (size ? size : 1);
}

size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream) {
    size_t total = size * nmemb;
    long ret = syscall3(SYS_WRITE, stream->fd, (long)ptr, total);
    if (ret < 0)
        return 0;
    return (size_t)ret / (size ? size : 1);
}

int fclose(FILE *stream) {
    if (!stream)
        return -1;
    int fd = stream->fd;
    free(stream);
    return (int)syscall3(SYS_CLOSE, fd, 0, 0);
}

int rename(const char *old, const char *new) {
    return (int)syscall3(SYS_RENAME, (long)old, (long)new, 0);
}

long ftell(FILE *stream) {
    if (!stream)
        return -1;
    return syscall3(SYS_LSEEK, stream->fd, 0, SEEK_CUR);
}

int fseek(FILE *stream, long offset, int whence) {
    if (!stream)
        return -1;
    long ret = syscall3(SYS_LSEEK, stream->fd, offset, whence);
    return ret < 0 ? -1 : 0;
}

__attribute__((weak)) int printf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    va_end(ap);
    (void)fmt;
    return 0;
}
