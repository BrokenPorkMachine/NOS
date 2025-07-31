#include "libc.h"
#include <stdint.h>

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

#define HEAP_SIZE (16 * 1024)
#define HEAP_MAGIC 0xC0DECAFE

typedef struct block_header {
    size_t size;
    int    free;
    struct block_header *next;
    uint32_t magic;
} block_header_t;

static uint8_t heap[HEAP_SIZE];
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
    (void)destlen; // parameter not used in this stub
    return memcpy(dest, src, n);
}

char *__strncpy_chk(char *dest, const char *src, size_t n, size_t destlen) {
    (void)destlen; // parameter not used in this stub
    return strncpy(dest, src, n);
}
