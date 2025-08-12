#include "heap.h"
#include "legacy_heap.h"
#include "nitroheap/nitroheap.h"
#include <string.h>

static int use_nitro =
#ifdef CONFIG_NITRO_HEAP
1;
#else
0;
#endif

void kheap_parse_bootarg(const char* cmdline) {
    if (!cmdline) return;
    if (strstr(cmdline, "heap=nitro"))
        use_nitro = 1;
    else if (strstr(cmdline, "heap=legacy"))
        use_nitro = 0;
}

void kheap_init(void) {
    if (use_nitro)
        nitroheap_init();
    else
        legacy_kheap_init();
}

void* kmalloc(size_t sz, size_t align) {
    if (use_nitro)
        return nitro_kmalloc(sz, align);
    return legacy_kmalloc(sz);
}

void kfree(void* p) {
    if (use_nitro)
        nitro_kfree(p);
    else
        legacy_kfree(p);
}

void* krealloc(void* p, size_t newsz, size_t align) {
    if (use_nitro)
        return nitro_krealloc(p, newsz, align);
    return legacy_krealloc(p, newsz);
}

void kheap_dump_stats(const char* tag) {
    if (use_nitro)
        nitro_kheap_dump_stats(tag);
    (void)tag; // legacy heap has no stats
}

void kheap_trim(void) {
    if (use_nitro)
        nitro_kheap_trim();
}
