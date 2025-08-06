#ifndef NOS_NOSM_H
#define NOS_NOSM_H

#include <stdint.h>
#include <stddef.h>

#define NOSM_MAGIC 0x4D534F4EU /* 'NOSM' in little endian */

/* Module header present at the beginning of every .nosm file */
typedef struct {
    uint32_t magic;       /* must be NOSM_MAGIC */
    uint16_t version;     /* format version */
    uint16_t num_segments;/* number of segment descriptors */
    uint32_t entry;       /* entrypoint relative to load base */
    /* Future expansion: symbol table offset/size, signature, etc. */
} __attribute__((packed)) nosm_header_t;

/* Segment descriptor describing a loadable region */
typedef struct {
    uint32_t vaddr;   /* virtual address to map/copy to */
    uint32_t size;    /* size of segment in bytes */
    uint32_t offset;  /* offset in file where segment data resides */
    uint8_t flags;    /* bit 0 = R, bit1 = W, bit2 = X */
    uint8_t reserved[3]; /* reserved for alignment */
} __attribute__((packed)) nosm_segment_t;

/* Load a NOSM image already present in memory.
 * Returns pointer to module entrypoint on success or NULL on error.
 */
void *nosm_load(const void *image, size_t size);

#endif /* NOS_NOSM_H */
