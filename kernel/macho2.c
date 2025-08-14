#include <macho2.h>
#include <stdint.h>
#include <string.h>

#define LC_SYMTAB   0x2

struct symtab_command {
    uint32_t cmd;
    uint32_t cmdsize;
    uint32_t symoff;
    uint32_t nsyms;
    uint32_t stroff;
    uint32_t strsize;
};

struct nlist_64 {
    uint32_t n_strx;
    uint8_t  n_type;
    uint8_t  n_sect;
    uint16_t n_desc;
    uint64_t n_value;
};

int macho2_load(const void *image, size_t size) {
    if (!image || size < sizeof(struct mach_header_64))
        return -1;
    const struct mach_header_64 *mh = (const struct mach_header_64 *)image;
    if (mh->magic != MH_MAGIC_64)
        return -1;
    return 0;
}

void *macho2_find_symbol(const void *image, size_t size, const char *name) {
    if (!image || !name || size < sizeof(struct mach_header_64))
        return NULL;

    const uint8_t *base = (const uint8_t *)image;
    const struct mach_header_64 *mh = (const struct mach_header_64 *)base;
    if (mh->magic != MH_MAGIC_64)
        return NULL;

    const uint8_t *p = base + sizeof(struct mach_header_64);
    const struct symtab_command *sc = NULL;

    for (uint32_t i = 0; i < mh->ncmds; i++) {
        if (p + sizeof(struct load_command) > base + size)
            return NULL;
        const struct load_command *lc = (const struct load_command *)p;
        if (lc->cmd == LC_SYMTAB && lc->cmdsize >= sizeof(struct symtab_command))
            sc = (const struct symtab_command *)lc;
        p += lc->cmdsize;
    }

    if (!sc)
        return NULL;
    if ((size_t)sc->symoff + sc->nsyms * sizeof(struct nlist_64) > size)
        return NULL;
    if ((size_t)sc->stroff + sc->strsize > size)
        return NULL;

    const struct nlist_64 *syms = (const struct nlist_64 *)(base + sc->symoff);
    const char *strtab = (const char *)(base + sc->stroff);

    for (uint32_t i = 0; i < sc->nsyms; i++) {
        if (syms[i].n_strx >= sc->strsize)
            continue;
        const char *sname = strtab + syms[i].n_strx;
        if (strcmp(sname, name) == 0) {
            uint64_t off = syms[i].n_value;
            if (off < size)
                return (void *)(base + off);
            return NULL;
        }
    }

    return NULL;
}

