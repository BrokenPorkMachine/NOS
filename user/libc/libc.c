#include "libc.h"
#include <stdint.h>
#include <stddef.h>
#include <time.h>

// ---- Your kernel-mode recursive spinlock mutex ----
extern uint32_t thread_self(void);

int pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr) {
    (void)attr;
    mutex->lock = 0;
    mutex->owner = (uint32_t)-1;
    mutex->count = 0;
    return 0;
}
int pthread_mutex_lock(pthread_mutex_t *mutex) {
    uint32_t self = thread_self ? thread_self() : 1;
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
    if (mutex->owner != (thread_self ? thread_self() : 1))
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
    size_t len = 0;
    while (s[len]) len++;
    return len;
}

char *strncpy(char *dest, const char *src, size_t n) {
    size_t i;
    for (i = 0; i < n && src[i]; ++i) dest[i] = src[i];
    for (; i < n; ++i) dest[i] = '\0';
    return dest;
}

size_t strlcpy(char *dst, const char *src, size_t size) {
    size_t srclen = strlen(src);
    if (size) {
        size_t copylen = (srclen >= size) ? size - 1 : srclen;
        memcpy(dst, src, copylen);
        dst[copylen] = '\0';
    }
    return srclen;
}

int strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) { s1++; s2++; }
    return *(const unsigned char *)s1 - *(const unsigned char *)s2;
}

int strncmp(const char *s1, const char *s2, size_t n) {
    for (size_t i = 0; i < n; ++i)
        if (s1[i] != s2[i] || !s1[i] || !s2[i])
            return (unsigned char)s1[i] - (unsigned char)s2[i];
    return 0;
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

extern uint32_t thread_self(void);

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

// ================== FILE I/O (NitrFS) ===================
// ... (Insert your FILE I/O implementation here) ...
