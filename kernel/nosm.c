#include "nosm.h"
#include "agent.h"
#include <string.h>
#include <stdint.h>
#ifndef KERNEL_BUILD
#include <sys/mman.h>
#endif

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
    if (size < needed ||
        hdr->manifest_offset + hdr->manifest_size > size)
        return NULL;

    const nosm_segment_t *segs =
        (const nosm_segment_t *)(data + sizeof(nosm_header_t));
    const nosm_manifest_t *manifest =
        (const nosm_manifest_t *)(data + hdr->manifest_offset);

    /* determine base address for entrypoint computation */
    uintptr_t base = UINTPTR_MAX;
    for (uint16_t i = 0; i < hdr->num_segments; ++i) {
        if (segs[i].vaddr < base)
            base = segs[i].vaddr;
    }

    for (uint16_t i = 0; i < hdr->num_segments; ++i) {
        const nosm_segment_t *s = &segs[i];
        if (s->offset + s->size > size)
            return NULL; /* segment goes past end of file */
        void *dest = (void *)(uintptr_t)s->vaddr;
        const void *src = data + s->offset;
        memcpy(dest, src, (size_t)s->size);
#ifndef KERNEL_BUILD
        int prot = 0;
        if (s->flags & NOSM_FLAG_R) prot |= PROT_READ;
        if (s->flags & NOSM_FLAG_W) prot |= PROT_WRITE;
        if (s->flags & NOSM_FLAG_X) prot |= PROT_EXEC;
        mprotect(dest, (size_t)s->size, prot);
#else
        (void)dest; (void)s; /* Kernel would configure page tables here */
#endif
    }

    void (*entry)(void) = (void (*)(void))(base + hdr->entry);

    n2_agent_t agent = {0};
    memcpy(agent.name, manifest->name, sizeof(agent.name));
    memcpy(agent.version, manifest->version, sizeof(agent.version));
    agent.entry = entry;
    agent.manifest = manifest;
    n2_agent_register(&agent);

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
