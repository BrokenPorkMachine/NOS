#include "elf.h"
#include <stdint.h>
#include <stddef.h>
#include "../../user/libc/libc.h" // for memcpy/memset

// Basic validation of ELF64 image
int elf_validate(const void *image) {
    const Elf64_Ehdr *eh = (const Elf64_Ehdr *)image;
    if (eh->e_ident[0] != 0x7F || eh->e_ident[1] != 'E' ||
        eh->e_ident[2] != 'L' || eh->e_ident[3] != 'F')
        return -1;
    if (eh->e_ident[4] != 2) // 64-bit
        return -1;
    if (eh->e_machine != 0x3E) // x86_64
        return -1;
    return 0;
}

// Load PT_LOAD segments into memory. Assumes identity mapping.
void *elf_load(const void *image) {
    if (elf_validate(image) != 0)
        return NULL;
    const uint8_t *base = (const uint8_t *)image;
    const Elf64_Ehdr *eh = (const Elf64_Ehdr *)base;
    const Elf64_Phdr *ph = (const Elf64_Phdr *)(base + eh->e_phoff);
    for (uint16_t i = 0; i < eh->e_phnum; ++i) {
        if (ph[i].p_type != 1) // PT_LOAD
            continue;
        uint8_t *dest = (uint8_t *)ph[i].p_vaddr;
        const uint8_t *src = base + ph[i].p_offset;
        memcpy(dest, src, (size_t)ph[i].p_filesz);
        if (ph[i].p_memsz > ph[i].p_filesz) {
            memset(dest + ph[i].p_filesz, 0,
                   (size_t)(ph[i].p_memsz - ph[i].p_filesz));
        }
    }
    return (void *)eh->e_entry;
}
