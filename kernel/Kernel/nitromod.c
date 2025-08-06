#include "nitromod.h"
#include "../../user/libc/libc.h" // for memcpy

static uint32_t nitromod_checksum(const uint8_t *data, size_t len) {
    uint32_t sum = 0;
    for (size_t i = 0; i < len; ++i)
        sum += data[i];
    return sum;
}

int nitromod_load(const void *image, size_t size,
                  const nitromod_symbol_t **symtab,
                  size_t *sym_count) {
    if (!image || size < sizeof(nitromod_header_t))
        return -1;
    const uint8_t *base = (const uint8_t *)image;
    const nitromod_header_t *hdr = (const nitromod_header_t *)base;

    if (hdr->magic != NITROMOD_MAGIC || hdr->version != NITROMOD_VERSION)
        return -1;

    size_t seg_table_end = (size_t)hdr->seg_offset +
                           (size_t)hdr->num_segments * sizeof(nitromod_segment_t);
    if (seg_table_end > size)
        return -1;

    if (hdr->sig_offset + hdr->sig_size > size)
        return -1;
    if (hdr->sig_size != sizeof(uint32_t))
        return -1;
    uint32_t expected = *(const uint32_t *)(base + hdr->sig_offset);
    uint32_t actual = nitromod_checksum(base, hdr->sig_offset);
    if (expected != actual)
        return -1;

    const nitromod_segment_t *segs = (const nitromod_segment_t *)(base + hdr->seg_offset);
    for (uint16_t i = 0; i < hdr->num_segments; ++i) {
        size_t end = (size_t)segs[i].offset + (size_t)segs[i].size;
        if (end > size)
            return -1;
        uint8_t *dest = (uint8_t *)(uintptr_t)segs[i].target;
        const uint8_t *src = base + segs[i].offset;
        if (!dest || !src)
            return -1;
        memcpy(dest, src, segs[i].size);
    }

    if (hdr->symtab_offset) {
        size_t symtab_end = (size_t)hdr->symtab_offset +
                            (size_t)hdr->symtab_count * sizeof(nitromod_symbol_t);
        if (symtab_end > size)
            return -1;
        if (symtab)
            *symtab = (const nitromod_symbol_t *)(base + hdr->symtab_offset);
        if (sym_count)
            *sym_count = hdr->symtab_count;
    } else {
        if (symtab)
            *symtab = NULL;
        if (sym_count)
            *sym_count = 0;
    }

    if (!hdr->entry)
        return -1;
    void (*entry)(void) = (void (*)(void))(uintptr_t)hdr->entry;
    entry();

    return 0;
}
