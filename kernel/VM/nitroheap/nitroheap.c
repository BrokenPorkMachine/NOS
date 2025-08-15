#include "nitroheap.h"
#include "classes.h"
#include "nitroheap_sys.h"
#include "../pmm_buddy.h"
#include "../../arch/CPU/smp.h"
#include <string.h>
#include <printf.h>
#include <stdatomic.h>

// Simple production-ready NitroHeap implementation.
// Provides size-class-based caching directly atop the buddy allocator.

typedef struct nh_span {
    struct nh_span* next;
    int class_idx;
    size_t total_blocks;
    size_t free_blocks;
    uint32_t order;      // buddy order used to allocate this span
} nh_span_t;

typedef struct nh_block_header {
    nh_span_t* span;   // NULL for big allocations
    size_t     size;   // class size or big allocation size
    uint32_t   order;  // buddy order for big allocations
    uint32_t   home_cpu;
    uint64_t   reuse_epoch; // epoch when block may be reused
} nh_block_header_t;

typedef struct nh_free_node {
    struct nh_free_node* next;
} nh_free_node_t;

#define NH_CLASS_LIMIT 64
#define NH_MAG_SIZE    16
#define NH_MAX_CPUS    32
#define NH_REUSE_DELAY 1

typedef struct {
    nh_free_node_t* head;
    size_t          count;
} nh_magazine_t;

typedef struct {
    _Atomic(nh_free_node_t*) remote[NH_MAX_CPUS]; // cross-CPU frees
    nh_free_node_t*          freelist;           // central freelist
    nh_span_t*               spans;
    nh_magazine_t            magazines[NH_MAX_CPUS]; // per-CPU magazines
} nh_class_state_t;
static nh_class_state_t nh_classes[NH_CLASS_LIMIT];

#define NH_LARGE_ORDERS 32
static nh_free_node_t* nh_large_freelists[NH_LARGE_ORDERS];

typedef struct { nh_free_node_t* head; } nh_quarantine_t;
static nh_quarantine_t nh_quarantine[NH_MAX_CPUS];
static _Atomic(nh_free_node_t*) nh_large_remote[NH_MAX_CPUS];
static uint64_t nh_epoch[NH_MAX_CPUS];

static void nh_quarantine_push(uint32_t cpu, nh_free_node_t* node) {
    node->next = nh_quarantine[cpu].head;
    nh_quarantine[cpu].head = node;
}

static void nh_quarantine_enqueue(uint32_t cpu, nh_free_node_t* node) {
    nh_block_header_t* bh = ((nh_block_header_t*)node) - 1;
    bh->reuse_epoch = nh_epoch[cpu] + NH_REUSE_DELAY;
    nh_quarantine_push(cpu, node);
}

static void nh_quarantine_harvest_large(uint32_t cpu) {
    nh_free_node_t* r = atomic_exchange(&nh_large_remote[cpu], NULL);
    while (r) {
        nh_free_node_t* next = r->next;
        r->next = nh_quarantine[cpu].head;
        nh_quarantine[cpu].head = r;
        r = next;
    }
}

static void nh_quarantine_drain(uint32_t cpu) {
    nh_free_node_t** cur = &nh_quarantine[cpu].head;
    uint64_t now = nh_epoch[cpu];
    while (*cur) {
        nh_block_header_t* bh = ((nh_block_header_t*)(*cur)) - 1;
        if (bh->reuse_epoch > now) {
            cur = &(*cur)->next;
            continue;
        }
        nh_free_node_t* node = *cur;
        *cur = node->next;
        if (bh->span) {
            nh_class_state_t* cs = &nh_classes[bh->span->class_idx];
            nh_magazine_t* mag = &cs->magazines[cpu];
            if (mag->count >= NH_MAG_SIZE) {
                node->next = cs->freelist;
                cs->freelist = node;
            } else {
                node->next = mag->head;
                mag->head = node;
                mag->count++;
            }
        } else {
            if (bh->order < NH_LARGE_ORDERS) {
                node->next = nh_large_freelists[bh->order];
                nh_large_freelists[bh->order] = node;
            } else {
                buddy_free(bh, bh->order, 0);
            }
        }
    }
}

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
        nh_free_node_t* rhead = atomic_exchange(&cs->remote[cpu], NULL);
        nh_free_node_t* keep = NULL;
        while (rhead) {
            nh_free_node_t* next = rhead->next;
            nh_block_header_t* bh = ((nh_block_header_t*)rhead) - 1;
            if (bh->span != sp) {
                rhead->next = keep;
                keep = rhead;
            }
            rhead = next;
        }
        while (keep) {
            nh_free_node_t* next = keep->next;
            nh_free_node_t* old = atomic_load(&cs->remote[cpu]);
            do {
                keep->next = old;
            } while (!atomic_compare_exchange_weak(&cs->remote[cpu], &old, keep));
            keep = next;
        }
        nh_free_node_t** qcur = &nh_quarantine[cpu].head;
        while (*qcur) {
            nh_block_header_t* qbh = ((nh_block_header_t*)(*qcur)) - 1;
            if (qbh->span == sp)
                *qcur = (*qcur)->next;
            else
                qcur = &(*qcur)->next;
        }
    }
}

static nh_span_t* nh_alloc_span(int cls) {
    nh_class_state_t* cs = &nh_classes[cls];
    size_t bsz = nh_size_classes[cls].size;
    size_t block_sz = sizeof(nh_block_header_t) + bsz;
    size_t span_bytes = PAGE_SIZE;
    size_t blocks = (span_bytes - sizeof(nh_span_t)) / block_sz;
    if (blocks == 0) {
        blocks = 1;
        span_bytes = sizeof(nh_span_t) + block_sz;
    } else {
        span_bytes = sizeof(nh_span_t) + blocks * block_sz;
    }

    uint32_t order = 0;
    size_t alloc_bytes = PAGE_SIZE;
    while (alloc_bytes < span_bytes) { alloc_bytes <<= 1; order++; }
    nh_span_t* sp = buddy_alloc(order, 0, 0);
    if (!sp) return NULL;
    sp->order = order;
    sp->next = cs->spans;
    cs->spans = sp;
    sp->class_idx = cls;
    // use full allocation
    blocks = (alloc_bytes - sizeof(nh_span_t)) / block_sz;
    sp->total_blocks = blocks;
    sp->free_blocks = blocks;

    char* ptr = (char*)(sp + 1);
    for (size_t i = 0; i < blocks; ++i) {
        nh_block_header_t* bh = (nh_block_header_t*)ptr;
        bh->span = sp;
        bh->size = bsz;
        bh->order = 0;
        nh_free_node_t* node = (nh_free_node_t*)(bh + 1);
        node->next = cs->freelist;
        cs->freelist = node;
        ptr += block_sz;
    }
    return sp;
}

void nitroheap_init(void) {
    memset(nh_classes, 0, sizeof(nh_class_state_t) * NH_CLASS_LIMIT);
    memset(nh_large_freelists, 0, sizeof(nh_large_freelists));
    memset(nh_quarantine, 0, sizeof(nh_quarantine));
    memset(nh_epoch, 0, sizeof(nh_epoch));
    for (size_t i = 0; i < NH_MAX_CPUS; ++i)
        atomic_store(&nh_large_remote[i], NULL);
}

void* nitro_kmalloc(size_t sz, size_t align) {
    int cls = nh_class_from_size(sz, align);
    uint32_t cpu = smp_cpu_index();
    if (cpu >= NH_MAX_CPUS) cpu = 0;
    nh_epoch[cpu]++;
    nh_quarantine_harvest_large(cpu);
    nh_quarantine_drain(cpu);
    if (cls < 0) {
        size_t total = sizeof(nh_block_header_t) + sz;
        size_t alloc_bytes = PAGE_SIZE;
        uint32_t order = 0;
        size_t min_align = align ? align : 1;
        while (alloc_bytes < total || alloc_bytes < min_align) { alloc_bytes <<= 1; order++; }
        nh_block_header_t* bh;
        if (order < NH_LARGE_ORDERS && nh_large_freelists[order]) {
            nh_free_node_t* n = nh_large_freelists[order];
            nh_large_freelists[order] = n->next;
            bh = ((nh_block_header_t*)n) - 1;
        } else {
            bh = buddy_alloc(order, 0, 0);
            if (!bh) return NULL;
        }
        bh->span = NULL;
        bh->size = sz;
        bh->order = order;
        bh->home_cpu = cpu;
        bh->reuse_epoch = 0;
        return bh + 1;
    }

    nh_class_state_t* cs = &nh_classes[cls];
    nh_magazine_t* mag = &cs->magazines[cpu];
    nh_free_node_t* rem = atomic_exchange(&cs->remote[cpu], NULL);
    while (rem) {
        nh_free_node_t* next = rem->next;
        nh_quarantine_push(cpu, rem);
        rem = next;
    }
    nh_quarantine_drain(cpu);
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
    bh->home_cpu = cpu;
    bh->reuse_epoch = 0;
    return node;
}

void nitro_kfree(void* p) {
    if (!p) return;
    uint32_t cpu = smp_cpu_index();
    if (cpu >= NH_MAX_CPUS) cpu = 0;
    nh_epoch[cpu]++;
    nh_block_header_t* bh = ((nh_block_header_t*)p) - 1;
    uint32_t home = bh->home_cpu;
    if (home >= NH_MAX_CPUS) home = 0;
    nh_free_node_t* node = (nh_free_node_t*)p;
    if (!bh->span) {
        if (home != cpu && home < NH_MAX_CPUS) {
            bh->reuse_epoch = nh_epoch[home] + NH_REUSE_DELAY;
            nh_free_node_t* head;
            do {
                head = atomic_load(&nh_large_remote[home]);
                node->next = head;
            } while (!atomic_compare_exchange_weak(&nh_large_remote[home], &head, node));
        } else {
            nh_quarantine_enqueue(home, node);
        }
        return;
    }
    nh_class_state_t* cs = &nh_classes[bh->span->class_idx];
    if (home != cpu && home < NH_MAX_CPUS) {
        bh->reuse_epoch = nh_epoch[home] + NH_REUSE_DELAY;
        nh_free_node_t* head;
        do {
            head = atomic_load(&cs->remote[home]);
            node->next = head;
        } while (!atomic_compare_exchange_weak(&cs->remote[home], &head, node));
    } else {
        nh_quarantine_enqueue(home, node);
    }
    bh->span->free_blocks++;
    if (bh->span->free_blocks == bh->span->total_blocks) {
        nh_remove_span_blocks(cs, bh->span);
        nh_span_t** cur = &cs->spans;
        while (*cur && *cur != bh->span) cur = &(*cur)->next;
        if (*cur == bh->span) *cur = bh->span->next;
        buddy_free(bh->span, bh->span->order, 0);
    }
}

void* nitro_krealloc(void* p, size_t newsz, size_t align) {
    if (!p) return nitro_kmalloc(newsz, align);
    if (!newsz) { nitro_kfree(p); return NULL; }

    nh_block_header_t* bh = ((nh_block_header_t*)p) - 1;
    if (!bh->span) {
        size_t oldsz = bh->size;
        if (newsz <= oldsz)
            return p;
        void* n = nitro_kmalloc(newsz, align);
        if (!n) return NULL;
        memcpy(n, p, oldsz < newsz ? oldsz : newsz);
        nitro_kfree(p);
        return n;
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
    for (size_t cpu = 0; cpu < NH_MAX_CPUS; ++cpu) {
        for (size_t i = 0; i < nh_size_class_count; ++i) {
            nh_class_state_t* cs = &nh_classes[i];
            nh_free_node_t* r = atomic_exchange(&cs->remote[cpu], NULL);
            while (r) {
                nh_free_node_t* next = r->next;
                nh_quarantine_push(cpu, r);
                r = next;
            }
        }
        nh_free_node_t* lr = atomic_exchange(&nh_large_remote[cpu], NULL);
        while (lr) {
            nh_free_node_t* next = lr->next;
            nh_quarantine_push(cpu, lr);
            lr = next;
        }
        nh_free_node_t* q = nh_quarantine[cpu].head;
        nh_quarantine[cpu].head = NULL;
        while (q) {
            nh_free_node_t* next = q->next;
            nh_block_header_t* bh = ((nh_block_header_t*)q) - 1;
            if (bh->span) {
                nh_class_state_t* cs = &nh_classes[bh->span->class_idx];
                q->next = cs->freelist;
                cs->freelist = q;
            } else {
                if (bh->order < NH_LARGE_ORDERS) {
                    q->next = nh_large_freelists[bh->order];
                    nh_large_freelists[bh->order] = q;
                } else {
                    buddy_free(bh, bh->order, 0);
                }
            }
            q = next;
        }
    }
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
                buddy_free(sp, sp->order, 0);
            }
            sp = next;
        }
    }
    for (size_t o = 0; o < NH_LARGE_ORDERS; ++o) {
        nh_free_node_t* n = nh_large_freelists[o];
        while (n) {
            nh_free_node_t* next = n->next;
            buddy_free(((nh_block_header_t*)n) - 1, o, 0);
            n = next;
        }
        nh_large_freelists[o] = NULL;
    }
}

static inline size_t nh_extract_align(nh_flags_t flags) {
    nh_flags_t f = flags & NH_ALIGN_MASK;
    if (!f) return 0;
    return (size_t)1 << (f >> NH_ALIGN_LOG2_SHIFT);
}

int sys_nh_alloc(const nh_alloc_req* in, nh_alloc_resp* out) {
    if (!in || !out) return -1;
    size_t align = nh_extract_align(in->flags);
    void* p = nitro_kmalloc(in->size, align);
    if (!p) return -1;
    out->ptr = p;
    return 0;
}

int sys_nh_free(const nh_free_req* in) {
    if (!in) return -1;
    nitro_kfree(in->ptr);
    return 0;
}

int sys_nh_realloc(const nh_realloc_req* in, nh_alloc_resp* out) {
    if (!in || !out) return -1;
    size_t align = nh_extract_align(in->flags);
    void* p = nitro_krealloc(in->ptr, in->new_size, align);
    if (!p && in->new_size) return -1;
    out->ptr = p;
    return 0;
}

int sys_nh_halloc(const nh_halloc_req* in, nh_halloc_resp* out) {
    (void)in; (void)out;
    return -1;
}

int sys_nh_hptr(const nh_hptr_req* in, nh_alloc_resp* out) {
    (void)in; (void)out;
    return -1;
}

int sys_nh_hfree(const nh_hfree_req* in) {
    (void)in;
    return -1;
}

