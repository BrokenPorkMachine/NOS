#include "libc.h"
#include <stdint.h>
#include "../../kernel/Kernel/syscall.h"
#include "../../kernel/Task/thread.h"
#include "../../kernel/IPC/ipc.h"
#include "../servers/nitrfs/server.h"
#include "../servers/nitrfs/nitrfs.h"

// Weak fallbacks so unit tests can link without full kernel.
__attribute__((weak)) ipc_queue_t fs_queue;
__attribute__((weak)) thread_t *current_cpu[MAX_CPUS];
__attribute__((weak)) uint32_t smp_cpu_index(void) { return 0; }
__attribute__((weak)) int ipc_send(ipc_queue_t *q, uint32_t s, ipc_message_t *m) { (void)q; (void)s; (void)m; return -1; }
__attribute__((weak)) int ipc_receive(ipc_queue_t *q, uint32_t r, ipc_message_t *m) { (void)q; (void)r; (void)m; return -1; }

static inline uint32_t self_id(void) {
    thread_t *t = current_cpu[smp_cpu_index()];
    return t ? (uint32_t)t->id : 0;
}

void *memset(void *s, int c, size_t n) {
    unsigned char *p = (unsigned char *)s;
    for (size_t i = 0; i < n; i++)
        p[i] = (unsigned char)c;
    return s;
}

void *memcpy(void *dest, const void *src, size_t n) {
    unsigned char *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;
    for (size_t i = 0; i < n; i++)
        d[i] = s[i];
    return dest;
}

void *memmove(void *dest, const void *src, size_t n) {
    unsigned char *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;
    if (d < s) {
        for (size_t i = 0; i < n; i++) d[i] = s[i];
    } else if (d > s) {
        for (size_t i = n; i != 0; i--) d[i-1] = s[i-1];
    }
    return dest;
}

int memcmp(const void *s1, const void *s2, size_t n) {
    const unsigned char *p1 = (const unsigned char *)s1;
    const unsigned char *p2 = (const unsigned char *)s2;
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
    for (i = 0; i < n && src[i]; ++i)
        dest[i] = src[i];
    for (; i < n; ++i)
        dest[i] = '\0';
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
    for (size_t i = 0; i < n; ++i) {
        if (s1[i] != s2[i] || !s1[i] || !s2[i])
            return (unsigned char)s1[i] - (unsigned char)s2[i];
    }
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
    while ((*d++ = *src++)) {
        /* copy including null terminator */
    }
    return dest;
}

char *strcat(char *dest, const char *src) {
    char *d = dest + strlen(dest);
    while ((*d++ = *src++)) {
        /* append */
    }
    return dest;
}

char *strstr(const char *haystack, const char *needle) {
    if (!*needle)
        return (char *)haystack;
    size_t nlen = strlen(needle);
    for (; *haystack; haystack++) {
        if (*haystack == *needle && strncmp(haystack, needle, nlen) == 0)
            return (char *)haystack;
    }
    return NULL;
}

#define HEAP_SIZE (64 * 1024)
#define HEAP_MAGIC 0xC0DECAFE

typedef struct block_header {
    size_t size;
    int    free;
    struct block_header *next;
    uint32_t magic;
} block_header_t;

static uint8_t __attribute__((aligned(16))) heap[HEAP_SIZE];
static block_header_t *free_list = NULL;

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
    if (!size)
        return NULL;
    if (!free_list)
        heap_init();
    size = (size + 7) & ~((size_t)7);
    block_header_t *block = find_block(size);
    if (!block)
        return NULL;
    split_block(block, size);
    block->free = 0;
    return (uint8_t *)block + sizeof(block_header_t);
}

void *calloc(size_t nmemb, size_t size) {
    if (size && nmemb > SIZE_MAX / size)
        return NULL;
    size_t total = nmemb * size;
    void *p = malloc(total);
    if (p)
        memset(p, 0, total);
    return p;
}

void free(void *ptr) {
    if (!ptr)
        return;
    block_header_t *block = (block_header_t *)((uint8_t *)ptr - sizeof(block_header_t));
    if (block->magic != HEAP_MAGIC || block->free)
        return;
    memset(ptr, 0, block->size);
    block->free = 1;

    block_header_t *cur = free_list;
    while (cur && cur->next) {
        if (cur->free && cur->next->free &&
            (uint8_t *)cur + sizeof(block_header_t) + cur->size == (uint8_t *)cur->next) {
            cur->size += sizeof(block_header_t) + cur->next->size;
            cur->next = cur->next->next;
            continue;
        }
        cur = cur->next;
    }
}
void *__memcpy_chk(void *dest, const void *src, size_t n, size_t destlen) {
    if (n > destlen)
        n = destlen;
    return memcpy(dest, src, n);
}

char *__strncpy_chk(char *dest, const char *src, size_t n, size_t destlen) {
    if (destlen == 0)
        return dest;
    if (n >= destlen)
        n = destlen - 1;
    strncpy(dest, src, n);
    dest[n] = '\0';
    return dest;
}

// --- File I/O using NitrFS IPC ---
static int fs_find_handle(const char *name) {
    ipc_message_t msg = {0}, reply = {0};
    msg.type = NITRFS_MSG_LIST;
    ipc_send(&fs_queue, self_id(), &msg);
    ipc_receive(&fs_queue, self_id(), &reply);
    for (int i = 0; i < (int)reply.arg1; i++) {
        char *n = (char *)reply.data + i * NITRFS_NAME_LEN;
        if (strncmp(n, name, NITRFS_NAME_LEN) == 0)
            return i;
    }
    return -1;
}

FILE *fopen(const char *path, const char *mode) {
    int handle = fs_find_handle(path);
    if (handle < 0 && mode && strchr(mode, 'w')) {
        ipc_message_t msg = {0}, reply = {0};
        msg.type = NITRFS_MSG_CREATE;
        msg.arg1 = IPC_MSG_DATA_MAX;
        msg.arg2 = NITRFS_PERM_READ | NITRFS_PERM_WRITE;
        size_t len = strlen(path);
        if (len > IPC_MSG_DATA_MAX - 1)
            len = IPC_MSG_DATA_MAX - 1;
        memcpy(msg.data, path, len);
        msg.data[len] = '\0';
        msg.len = len;
        ipc_send(&fs_queue, self_id(), &msg);
        ipc_receive(&fs_queue, self_id(), &reply);
        handle = reply.arg1;
    }
    if (handle < 0)
        return NULL;
    FILE *f = malloc(sizeof(FILE));
    if (!f)
        return NULL;
    f->handle = handle;
    f->pos = 0;
    return f;
}

size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream) {
    size_t len = size * nmemb;
    if (len > IPC_MSG_DATA_MAX)
        len = IPC_MSG_DATA_MAX;
    ipc_message_t msg = {0}, reply = {0};
    msg.type = NITRFS_MSG_READ;
    msg.arg1 = stream->handle;
    msg.arg2 = stream->pos;
    msg.len  = len;
    ipc_send(&fs_queue, self_id(), &msg);
    ipc_receive(&fs_queue, self_id(), &reply);
    if (reply.arg1 != 0)
        return 0;
    memcpy(ptr, reply.data, reply.len);
    stream->pos += reply.len;
    return reply.len / size;
}

size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream) {
    size_t len = size * nmemb;
    if (len > IPC_MSG_DATA_MAX)
        len = IPC_MSG_DATA_MAX;
    ipc_message_t msg = {0}, reply = {0};
    msg.type = NITRFS_MSG_WRITE;
    msg.arg1 = stream->handle;
    msg.arg2 = stream->pos;
    memcpy(msg.data, ptr, len);
    msg.len = len;
    ipc_send(&fs_queue, self_id(), &msg);
    ipc_receive(&fs_queue, self_id(), &reply);
    if (reply.arg1 != 0)
        return 0;
    stream->pos += len;
    return len / size;
}

int fclose(FILE *stream) {
    if (!stream)
        return -1;
    free(stream);
    return 0;
}

int rename(const char *old, const char *new) {
    int handle = fs_find_handle(old);
    if (handle < 0)
        return -1;
    ipc_message_t msg = {0}, reply = {0};
    msg.type = NITRFS_MSG_RENAME;
    msg.arg1 = handle;
    size_t len = strlen(new);
    if (len > IPC_MSG_DATA_MAX - 1)
        len = IPC_MSG_DATA_MAX - 1;
    memcpy(msg.data, new, len);
    msg.data[len] = '\0';
    msg.len = len;
    ipc_send(&fs_queue, self_id(), &msg);
    ipc_receive(&fs_queue, self_id(), &reply);
    return (int)reply.arg1;
}

long ftell(FILE *stream) {
    if (!stream)
        return -1;
    return (long)stream->pos;
}

int fseek(FILE *stream, long offset, int whence) {
    if (!stream)
        return -1;
    long base = 0;
    if (whence == SEEK_SET) {
        base = 0;
    } else if (whence == SEEK_CUR) {
        base = (long)stream->pos;
    } else {
        return -1; // SEEK_END not supported
    }
    long newpos = base + offset;
    if (newpos < 0)
        return -1;
    stream->pos = (unsigned int)newpos;
    return 0;
}

// --- Math helpers ---
int abs(int x) { return x < 0 ? -x : x; }
long labs(long x) { return x < 0 ? -x : x; }
long long llabs(long long x) { return x < 0 ? -x : x; }
double sqrt(double x) {
    if (x <= 0)
        return 0;
    double r = x;
    for (int i = 0; i < 20; ++i)
        r = 0.5 * (r + x / r);
    return r;
}

// --- System call wrappers ---
static inline long syscall3(long n, long a1, long a2, long a3) {
    long ret;
    asm volatile("mov %1, %%rax; mov %2, %%rdi; mov %3, %%rsi; mov %4, %%rdx; int $0x80; mov %%rax, %0"
                 : "=r"(ret)
                 : "r"(n), "r"(a1), "r"(a2), "r"(a3)
                 : "rax", "rdi", "rsi", "rdx");
    return ret;
}

int fork(void) {
    return (int)syscall3(SYS_FORK, 0, 0, 0);
}

int exec(const char *path) {
    return (int)syscall3(SYS_EXEC, (long)path, 0, 0);
}

void *sbrk(long inc) {
    return (void *)syscall3(SYS_SBRK, inc, 0, 0);
}
