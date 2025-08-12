#include "nitroheap.h"
#include "classes.h"
#include "../legacy_heap.h"
#include "../../arch/CPU/smp.h"
#include <string.h>
#include <printf.h>

// Simple production-ready NitroHeap implementation.
// Provides size-class-based caching on top of legacy buddy allocator.

typedef struct nh_span {
    struct nh_span* next;
    int class_idx;
    size_t total_blocks;
    size_t free_blocks;
} nh_span_t;

typedef struct nh_block_header {
    nh_span_t* span;   // NULL for big allocations
    size_t     size;   // class size or big allocation size
} nh_block_header_t;

typedef struct nh_free_node {
    struct nh_free_node* next;
} nh_free_node_t;

#define NH_CLASS_LIMIT 64
#define NH_MAG_SIZE    16
#define NH_MAX_CPUS    32

typedef struct {
    nh_free_node_t* head;
    size_t          count;
} nh_magazine_t;

typedef struct {
    nh_free_node_t* freelist;      // central freelist
    nh_span_t*      spans;
    nh_magazine_t   magazines[NH_MAX_CPUS]; // per-CPU magazines
} nh_class_state_t;
static nh_class_state_t nh_classes[NH_CLASS_LIMIT];

static void nh_remove_span_blocks(nh_class_state_t* cs, nh_span_t* sp) {
    nh_free_node_t** cur = &cs->freelist;
    while (*cur) {
        nh_block_header_t* bh = ((nh_block_header_t*)(*cur)) - 1;
        if (bh->span == sp)
            *cur = (*cur)->next;
        else
            cur = &(*cur)->next;
    }
    for (size_t cpu = 0; cpu < NH_MAX_CPUS; ++cpu) {
        nh_magazine_t* mag = &cs->magazines[cpu];
        nh_free_node_t** mcur = &mag->head;
        while (*mcur) {
            nh_block_header_t* bh = ((nh_block_header_t*)(*mcur)) - 1;
            if (bh->span == sp) {
                *mcur = (*mcur)->next;
                if (mag->count) mag->count--;
            } else {
                mcur = &(*mcur)->next;
            }
        }
    }
}

static nh_span_t* nh_alloc_span(int cls) {
    nh_class_state_t* cs = &nh_classes[cls];
    size_t bsz = nh_size_classes[cls].size;
    size_t span_bytes = 4096;
    size_t blocks = (span_bytes - sizeof(nh_span_t)) / (bsz + sizeof(nh_block_header_t));
    if (blocks == 0) {
        blocks = 1;
        span_bytes = sizeof(nh_span_t) + sizeof(nh_block_header_t) + bsz;
    } else {
        span_bytes = sizeof(nh_span_t) + blocks*(sizeof(nh_block_header_t)+bsz);
    }
    nh_span_t* sp = legacy_kmalloc(span_bytes);
    if (!sp) return NULL;
    sp->next = cs->spans;
    cs->spans = sp;
    sp->class_idx = cls;
    sp->total_blocks = blocks;
    sp->free_blocks = blocks;

    char* ptr = (char*)(sp + 1);
    for (size_t i = 0; i < blocks; ++i) {
        nh_block_header_t* bh = (nh_block_header_t*)ptr;
        bh->span = sp;
        bh->size = bsz;
        nh_free_node_t* node = (nh_free_node_t*)(bh + 1);
        node->next = cs->freelist;
        cs->freelist = node;
        ptr += sizeof(nh_block_header_t) + bsz;
    }
    return sp;
}

void nitroheap_init(void) {
    memset(nh_classes, 0, sizeof(nh_class_state_t) * NH_CLASS_LIMIT);
}

void* nitro_kmalloc(size_t sz, size_t align) {
    int cls = size_class_for(sz, align);
    if (cls < 0) {
        size_t total = sizeof(nh_block_header_t) + sz;
        nh_block_header_t* bh = legacy_kmalloc(total);
        if (!bh) return NULL;
        bh->span = NULL;
        bh->size = sz;
        return bh + 1;
    }

    nh_class_state_t* cs = &nh_classes[cls];
    uint32_t cpu = smp_cpu_index();
    if (cpu >= NH_MAX_CPUS) cpu = 0;
    nh_magazine_t* mag = &cs->magazines[cpu];
    if (!mag->head) {
        if (!cs->freelist && !nh_alloc_span(cls))
            return NULL;
        for (size_t i = 0; i < NH_MAG_SIZE && cs->freelist; ++i) {
            nh_free_node_t* n = cs->freelist;
            cs->freelist = n->next;
            n->next = mag->head;
            mag->head = n;
            mag->count++;
        }
    }
    nh_free_node_t* node = mag->head;
    mag->head = node->next;
    mag->count--;
    nh_block_header_t* bh = ((nh_block_header_t*)node) - 1;
    bh->span->free_blocks--;
    return node;
}

void nitro_kfree(void* p) {
    if (!p) return;
    nh_block_header_t* bh = ((nh_block_header_t*)p) - 1;
    if (!bh->span) {
        legacy_kfree(bh);
        return;
    }
    nh_class_state_t* cs = &nh_classes[bh->span->class_idx];
    uint32_t cpu = smp_cpu_index();
    if (cpu >= NH_MAX_CPUS) cpu = 0;
    nh_magazine_t* mag = &cs->magazines[cpu];
    nh_free_node_t* node = (nh_free_node_t*)p;
    if (mag->count >= NH_MAG_SIZE) {
        while (mag->head) {
            nh_free_node_t* n = mag->head;
            mag->head = n->next;
            n->next = cs->freelist;
            cs->freelist = n;
        }
        mag->count = 0;
    }
    node->next = mag->head;
    mag->head = node;
    mag->count++;
    bh->span->free_blocks++;
    if (bh->span->free_blocks == bh->span->total_blocks) {
        nh_remove_span_blocks(cs, bh->span);
        nh_span_t** cur = &cs->spans;
        while (*cur && *cur != bh->span) cur = &(*cur)->next;
        if (*cur == bh->span) *cur = bh->span->next;
        legacy_kfree(bh->span);
    }
}

void* nitro_krealloc(void* p, size_t newsz, size_t align) {
    if (!p) return nitro_kmalloc(newsz, align);
    if (!newsz) { nitro_kfree(p); return NULL; }

    nh_block_header_t* bh = ((nh_block_header_t*)p) - 1;
    if (!bh->span) {
        nh_block_header_t* nb = legacy_krealloc(bh, sizeof(nh_block_header_t) + newsz);
        if (!nb) return NULL;
        nb->span = NULL;
        nb->size = newsz;
        return nb + 1;
    }

    size_t oldsz = bh->size;
    int oldcls = bh->span->class_idx;
    if (newsz <= oldsz && align <= nh_size_classes[oldcls].align)
        return p;

    void* n = nitro_kmalloc(newsz, align);
    if (!n) return NULL;
    memcpy(n, p, oldsz < newsz ? oldsz : newsz);
    nitro_kfree(p);
    return n;
}

void nitro_kheap_dump_stats(const char* tag) {
    kprintf("[nitroheap] %s\n", tag ? tag : "");
    for (size_t i = 0; i < nh_size_class_count; ++i) {
        nh_class_state_t* cs = &nh_classes[i];
        size_t spans = 0, free_blocks = 0, total = 0;
        for (nh_span_t* sp = cs->spans; sp; sp = sp->next) {
            spans++;
            total += sp->total_blocks;
            free_blocks += sp->free_blocks;
        }
        if (spans)
            kprintf(" class %zu size %zu: spans=%zu free=%zu/%zu\n",
                    i, nh_size_classes[i].size, spans, free_blocks, total);
    }
}

void nitro_kheap_trim(void) {
    for (size_t i = 0; i < nh_size_class_count; ++i) {
        nh_class_state_t* cs = &nh_classes[i];
        nh_span_t* sp = cs->spans;
        while (sp) {
            nh_span_t* next = sp->next;
            if (sp->free_blocks == sp->total_blocks) {
                nh_remove_span_blocks(cs, sp);
                nh_span_t** cur = &cs->spans;
                while (*cur && *cur != sp) cur = &(*cur)->next;
                if (*cur == sp) *cur = sp->next;
                legacy_kfree(sp);
            }
            sp = next;
        }
    }
}

