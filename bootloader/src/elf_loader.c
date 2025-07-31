// src/elf_loader.c
#include "../include/efi.h"
#include "../include/bootinfo.h"
#include "elf_loader.h"

// Minimal memcmp and memset, UEFI-safe (no libc!)
static int memcmp(const void *a, const void *b, UINTN n) {
    const unsigned char *p = (const unsigned char*)a, *q = (const unsigned char*)b;
    for (UINTN i = 0; i < n; ++i)
        if (p[i] != q[i]) return p[i] - q[i];
    return 0;
}
static void *memset(void *b, int v, UINTN n) {
    unsigned char *p = (unsigned char*)b;
    for (UINTN i = 0; i < n; ++i) p[i] = (unsigned char)v;
    return b;
}

// Print a 64-bit hex value to UEFI console (fallback)
static void print_hex(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *ConOut, const CHAR16 *prefix, UINT64 val) {
    CHAR16 buf[32];
    int idx = 0;
    if (prefix) while (prefix[idx]) { buf[idx] = prefix[idx]; idx++; }
    buf[idx++] = L'0'; buf[idx++] = L'x';
    int sh = 60;
    for (int i = 0; i < 16; ++i, sh -= 4)
        buf[idx++] = L"0123456789ABCDEF"[(val >> sh) & 0xF];
    buf[idx++] = L'\r'; buf[idx++] = L'\n'; buf[idx] = 0;
    ConOut->OutputString(ConOut, buf);
}

// Pure C version of EFI_SIZE_TO_PAGES
static UINTN size_to_pages(UINTN sz) {
    return (sz + 4095) / 4096;
}

// ---- ELF types ----
typedef struct {
    unsigned char e_ident[16];
    uint16_t e_type, e_machine;
    uint32_t e_version;
    uint64_t e_entry, e_phoff, e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize, e_phentsize, e_phnum, e_shentsize, e_shnum, e_shstrndx;
} __attribute__((packed)) Elf64_Ehdr;

typedef struct {
    uint32_t p_type, p_flags;
    uint64_t p_offset, p_vaddr, p_paddr, p_filesz, p_memsz, p_align;
} __attribute__((packed)) Elf64_Phdr;

#define PT_LOAD 1

EFI_STATUS load_and_boot_kernel(EFI_FILE_PROTOCOL *kernel_file, bootinfo_t *bootinfo,
                               EFI_BOOT_SERVICES *BS, EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *ConOut)
{
    EFI_STATUS st;
    UINTN sz;
    Elf64_Ehdr eh;

    // --- Read ELF header ---
    sz = sizeof(eh);
    st = kernel_file->Read(kernel_file, &sz, &eh);
    if (st || sz != sizeof(eh)) return EFI_LOAD_ERROR;

    // ELF magic check: 0x7F 'E' 'L' 'F'
    if (!(eh.e_ident[0] == 0x7F && eh.e_ident[1] == 'E' &&
          eh.e_ident[2] == 'L' && eh.e_ident[3] == 'F' && eh.e_ident[4] == 2)) // ELFCLASS64
        return EFI_LOAD_ERROR;

    // --- Load each PT_LOAD segment ---
    for (UINTN i = 0; i < eh.e_phnum; ++i) {
        Elf64_Phdr ph;
        UINT64 ph_offset = eh.e_phoff + i * sizeof(ph);
        st = kernel_file->SetPosition(kernel_file, ph_offset);
        if (st) return EFI_LOAD_ERROR;

        sz = sizeof(ph);
        st = kernel_file->Read(kernel_file, &sz, &ph);
        if (st || sz != sizeof(ph)) return EFI_LOAD_ERROR;

        if (ph.p_type != PT_LOAD || ph.p_memsz == 0) continue;

        EFI_PHYSICAL_ADDRESS dest = ph.p_paddr;
        UINTN pages = size_to_pages(ph.p_memsz);
        st = BS->AllocatePages(EFI_ALLOCATE_ADDRESS, EfiLoaderData, pages, &dest);
        if (st || dest != ph.p_paddr) {
            print_hex(ConOut, L"Segment alloc fail at ", ph.p_paddr);
            return EFI_LOAD_ERROR;
        }

        st = kernel_file->SetPosition(kernel_file, ph.p_offset);
        if (st) return EFI_LOAD_ERROR;

        sz = ph.p_filesz;
        st = kernel_file->Read(kernel_file, &sz, (void *)(UINTN)dest);
        if (st || sz != ph.p_filesz) return EFI_LOAD_ERROR;

        if (ph.p_memsz > ph.p_filesz)
            memset((void *)(UINTN)(dest + ph.p_filesz), 0, ph.p_memsz - ph.p_filesz);
    }

    EFI_PHYSICAL_ADDRESS entry = eh.e_entry;
    bootinfo->kernel_entry = (void *)(UINTN)entry;
    print_hex(ConOut, L"Kernel entry: ", entry);

    // Actually jump to kernel entry
    ((void (*)(bootinfo_t *))(UINTN)entry)(bootinfo);
    return EFI_SUCCESS; // never returns
}
