#ifndef NITROMOD_H
#define NITROMOD_H

#include <stddef.h>
#include <stdint.h>

#define NITROMOD_MAGIC 0x4E4D4F44 /* 'NMOD' */
#define NITROMOD_VERSION 1
#define NITROMOD_MAX_NAME 32

typedef struct {
    uint64_t target;      /* load address */
    uint32_t offset;      /* offset from start of image */
    uint32_t size;        /* number of bytes */
} nitromod_segment_t;

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t num_segments;
    uint64_t entry;
    uint32_t seg_offset;      /* offset to segment descriptors */
    uint32_t symtab_offset;   /* offset to symbol table */
    uint32_t symtab_count;    /* number of symbols */
    uint32_t sig_offset;      /* offset to signature */
    uint32_t sig_size;        /* size of signature */
} nitromod_header_t;

typedef struct {
    uint64_t addr;
    char name[NITROMOD_MAX_NAME];
} nitromod_symbol_t;

/*
 * Validate and load a NITROMOD image. On success, segments are copied to their
 * target addresses, the optional symbol table pointer and count are returned,
 * the digital signature is verified, and the module entrypoint is called.
 * Returns 0 on success or -1 on failure.
 */
int nitromod_load(const void *image, size_t size,
                  const nitromod_symbol_t **symtab,
                  size_t *sym_count);

#endif /* NITROMOD_H */
