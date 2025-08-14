#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "macho2.h"

void *macho2_find_symbol(const void *, size_t, const char *);

#define LC_SYMTAB 0x2
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
    uint8_t n_type;
    uint8_t n_sect;
    uint16_t n_desc;
    uint64_t n_value;
};

int main(void) {
    uint8_t image[256] = {0};
    struct mach_header_64 *mh = (struct mach_header_64 *)image;
    mh->magic = MH_MAGIC_64;
    mh->ncmds = 1;
    mh->sizeofcmds = sizeof(struct symtab_command);

    struct symtab_command *sc = (struct symtab_command *)(image + sizeof(struct mach_header_64));
    sc->cmd = LC_SYMTAB;
    sc->cmdsize = sizeof(struct symtab_command);
    sc->symoff = sizeof(struct mach_header_64) + sizeof(struct symtab_command);
    sc->nsyms = 1;
    sc->stroff = sc->symoff + sizeof(struct nlist_64);
    sc->strsize = 16;

    struct nlist_64 *nl = (struct nlist_64 *)(image + sc->symoff);
    nl->n_strx = 1; /* index into string table */
    nl->n_value = sc->stroff + sc->strsize; /* symbol points past string table */

    char *strtab = (char *)(image + sc->stroff);
    strtab[0] = '\0';
    strcpy(strtab + 1, "target");

    uint8_t *target = image + nl->n_value;
    *target = 0xAA;

    void *res = macho2_find_symbol(image, sizeof(image), "target");
    assert(res == target);
    return 0;
}
