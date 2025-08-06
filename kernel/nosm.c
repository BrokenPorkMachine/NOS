#include "nosm.h"
#include <string.h>
#include <stdint.h>

/* Optional memory protection constants */
#define NOSM_FLAG_R 0x1
#define NOSM_FLAG_W 0x2
#define NOSM_FLAG_X 0x4

/*
 * nosm_load - load a NOSM module image already present in memory.
 * The loader assumes that the kernel has already reserved/allocated
 * the target virtual memory areas described by each segment.
 */
void *nosm_load(const void *image, size_t size)
{
    if (!image || size < sizeof(nosm_header_t))
        return NULL;

    const uint8_t *data = (const uint8_t *)image;
    const nosm_header_t *hdr = (const nosm_header_t *)data;

    if (hdr->magic != NOSM_MAGIC || hdr->version != 1)
        return NULL;

    size_t needed = sizeof(nosm_header_t) +
                    (size_t)hdr->num_segments * sizeof(nosm_segment_t);
    if (size < needed)
        return NULL;

    const nosm_segment_t *segs =
        (const nosm_segment_t *)(data + sizeof(nosm_header_t));

    /* determine base address for entrypoint computation */
    uintptr_t base = UINTPTR_MAX;
    for (uint16_t i = 0; i < hdr->num_segments; ++i) {
        if (segs[i].vaddr < base)
            base = segs[i].vaddr;
    }

    for (uint16_t i = 0; i < hdr->num_segments; ++i) {
        const nosm_segment_t *s = &segs[i];
        if ((size_t)s->offset + s->size > size)
            return NULL; /* segment goes past end of file */
        void *dest = (void *)(uintptr_t)s->vaddr;
        const void *src = data + s->offset;
        memcpy(dest, src, s->size);
        /* TODO: apply memory permissions according to s->flags */
    }

    void (*entry)(void) = (void (*)(void))(base + hdr->entry);
    entry();
    return (void *)entry;
}

/* Example stub module entry point */
void nosm_example_entry(void)
{
    /* Module initialization code would live here */
}

/*
 * Creating a .nosm file from an ELF executable could be done using a
 * conversion script that reads the ELF program headers and writes out
 * this custom header followed by segment descriptors and data.
 * objcopy or a custom Python script using pyelftools would be suitable
 * starting points.
 */

/*
 * Possible kernel loader API:
 *   nosm_module_t *nosm_module_load(const void *img, size_t sz);
 *   void nosm_module_unload(nosm_module_t *module);
 *
 * A userland command could expose this functionality:
 *   $ nosmctl load mydriver.nosm
 *   $ nosmctl unload mydriver
 *
 * Future extensions might add exported symbol tables for linking
 * modules together or a digital signature block for authenticity.
 */
