// src/elf_loader.c
#include <efilib.h>
#include <elf.h>
#include "../include/efi.h"
#include "../include/bootinfo.h"
#include "elf_loader.h"

// Loads an ELF64 kernel from an open EFI_FILE_HANDLE, maps all PT_LOAD segments, and jumps to entry point.
// On success, does not return (kernel is executed). On error, returns EFI_LOAD_ERROR.
EFI_STATUS load_and_boot_kernel(EFI_FILE_HANDLE kernel_file, bootinfo_t *bootinfo)
{
    EFI_STATUS  st;
    UINTN       sz;
    Elf64_Ehdr  eh;

    // --- read ELF header ---
    sz = sizeof(eh);
    st = kernel_file->Read(kernel_file, &sz, &eh);
    if (EFI_ERROR(st) || sz != sizeof(eh))
        return EFI_LOAD_ERROR;
    if (memcmp(eh.e_ident, ELFMAG, SELFMAG) != 0 ||
        eh.e_ident[EI_CLASS] != ELFCLASS64)
        return EFI_LOAD_ERROR;

    // --- load each PT_LOAD segment ---
    for (UINTN i = 0; i < eh.e_phnum; ++i) {
        Elf64_Phdr ph;
        st = kernel_file->SetPosition(kernel_file,
                                      eh.e_phoff + i * sizeof(ph));
        if (EFI_ERROR(st))
            return st;

        sz = sizeof(ph);
        st = kernel_file->Read(kernel_file, &sz, &ph);
        if (EFI_ERROR(st) || sz != sizeof(ph))
            return EFI_LOAD_ERROR;

        if (ph.p_type != PT_LOAD || ph.p_memsz == 0)
            continue;

        EFI_PHYSICAL_ADDRESS dest = ph.p_paddr;
        UINTN pages = EFI_SIZE_TO_PAGES(ph.p_memsz);
        st = gBS->AllocatePages(AllocateAddress, EfiLoaderData, pages, &dest);
        if (EFI_ERROR(st) || dest != ph.p_paddr) {
            Print(L"Alloc pages failed for segment %u: %r\n", i, st);
            return EFI_LOAD_ERROR;
        }

        st = kernel_file->SetPosition(kernel_file, ph.p_offset);
        if (EFI_ERROR(st))
            return st;

        sz = ph.p_filesz;
        st = kernel_file->Read(kernel_file, &sz, (void *)(UINTN)dest);
        if (EFI_ERROR(st) || sz != ph.p_filesz)
            return EFI_LOAD_ERROR;

        if (ph.p_memsz > ph.p_filesz) {
            SetMem((void *)(UINTN)(dest + ph.p_filesz),
                   ph.p_memsz - ph.p_filesz, 0);
        }
    }

    EFI_PHYSICAL_ADDRESS entry = eh.e_entry;
    bootinfo->kernel_entry = (void *)(UINTN)entry;

    Print(L"Kernel entry = %lx\n", entry);
    Print(L"Bytes at entry: %02x %02x %02x %02x\n",
          *(UINT8 *)(UINTN)(entry + 0),
          *(UINT8 *)(UINTN)(entry + 1),
          *(UINT8 *)(UINTN)(entry + 2),
          *(UINT8 *)(UINTN)(entry + 3));

    // Jump to the kernel entry with bootinfo
    ((void (*)(bootinfo_t *))(UINTN)entry)(bootinfo);
    return EFI_SUCCESS; // Should not return
}
