#include "elf.h"
#include <stdint.h>
#include <stddef.h>
#include "../../user/libc/libc.h" // for memcpy/memset

// Validate ELF64 image: 0 = OK, -1 = invalid
int elf_validate(const void *image) {
    if (!image) return -1;
    const Elf64_Ehdr *eh = (const Elf64_Ehdr *)image;
    if (eh->e_ident[0] != 0x7F || eh->e_ident[1] != 'E' ||
        eh->e_ident[2] != 'L' || eh->e_ident[3] != 'F')
        return -1;
    if (eh->e_ident[4] != 2) // 64-bit
        return -1;
    if (eh->e_machine != 0x3E) // x86_64
        return -1;
    if (eh->e_phoff == 0 || eh->e_phnum == 0) // Must have program header(s)
        return -1;
    if (eh->e_phentsize != sizeof(Elf64_Phdr)) // Must match struct size
        return -1;
    return 0;
}

// Load PT_LOAD segments into memory. Assumes identity mapping.
// Returns entry point on success, NULL on failure.
void *elf_load(const void *image) {
    if (elf_validate(image) != 0)
        return NULL;
    const uint8_t *base = (const uint8_t *)image;
    const Elf64_Ehdr *eh = (const Elf64_Ehdr *)base;
    const Elf64_Phdr *ph = (const Elf64_Phdr *)(base + eh->e_phoff);

    for (uint16_t i = 0; i < eh->e_phnum; ++i) {
        if (ph[i].p_type != 1) // PT_LOAD
            continue;
        uint8_t *dest = (uint8_t *)(uintptr_t)ph[i].p_vaddr;
        const uint8_t *src = base + ph[i].p_offset;
        // Defensive: don't copy if size is zero or dest is NULL
        if (ph[i].p_filesz > 0 && dest && src)
            memcpy(dest, src, (size_t)ph[i].p_filesz);
        // Zero out .bss and other uninitialized
        if (ph[i].p_memsz > ph[i].p_filesz && dest)
            memset(dest + ph[i].p_filesz, 0, (size_t)(ph[i].p_memsz - ph[i].p_filesz));
    }
    return (void *)(uintptr_t)eh->e_entry;
}
