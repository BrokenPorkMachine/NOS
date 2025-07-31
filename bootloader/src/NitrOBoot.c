// src/NitrOBoot.c
#include "../include/efi.h"
#include "../include/bootinfo.h"

#define KERNEL_PATH L"\\EFI\\BOOT\\kernel.bin"
#define KERNEL_MAX_SIZE (2 * 1024 * 1024)
#define MAX_MMAP_ENTRIES 128

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

// Local memory functions
VOID *EFIAPI CopyMem(VOID *d, const VOID *s, UINTN l) {
    UINT8 *dst = (UINT8*)d; const UINT8 *src = (const UINT8*)s;
    for (UINTN i = 0; i < l; ++i) dst[i] = src[i];
    return d;
}
VOID *EFIAPI SetMem(VOID *b, UINTN l, UINT8 v) {
    UINT8 *buf = (UINT8*)b; for (UINTN i = 0; i < l; ++i) buf[i] = v; return b;
}

EFI_STATUS efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    EFI_STATUS status;
    EFI_BOOT_SERVICES *BS = SystemTable->BootServices;
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *ConOut = SystemTable->ConOut;

    // --- 1. Bootinfo struct ---
    bootinfo_t *info = NULL;
    BS->AllocatePages(EFI_ALLOCATE_ANY_PAGES, EfiLoaderData, 1, (EFI_PHYSICAL_ADDRESS*)&info);
    SetMem(info, sizeof(bootinfo_t), 0);
    info->magic = BOOTINFO_MAGIC_UEFI;
    info->size = sizeof(bootinfo_t);
    info->bootloader_name = "NitrOBoot UEFI";

    // --- 2. Memory map (also print to UEFI console) ---
    UINTN mmap_size = 0, map_key, desc_size;
    UINT32 desc_ver;
    BS->GetMemoryMap(&mmap_size, NULL, &map_key, &desc_size, &desc_ver);
    mmap_size += desc_size * 16;
    EFI_MEMORY_DESCRIPTOR *efi_mmap;
    BS->AllocatePages(EFI_ALLOCATE_ANY_PAGES, EfiLoaderData, (mmap_size + 4095) / 4096, (EFI_PHYSICAL_ADDRESS*)&efi_mmap);
    status = BS->GetMemoryMap(&mmap_size, efi_mmap, &map_key, &desc_size, &desc_ver);

    // Copy and print regions
    bootinfo_memory_t *mmap = NULL;
    BS->AllocatePages(EFI_ALLOCATE_ANY_PAGES, EfiLoaderData, 1, (EFI_PHYSICAL_ADDRESS*)&mmap);
    uint32_t mmap_count = 0;
    for (UINT8 *p = (UINT8*)efi_mmap; p < (UINT8*)efi_mmap + mmap_size; p += desc_size) {
        EFI_MEMORY_DESCRIPTOR *d = (EFI_MEMORY_DESCRIPTOR*)p;
        mmap[mmap_count].addr = d->PhysicalStart;
        mmap[mmap_count].len  = d->NumberOfPages * 4096;
        mmap[mmap_count].type = d->Type;
        mmap[mmap_count].reserved = 0;

        // Print region
        CHAR16 buf[128];
        SPrint(buf, sizeof(buf), L"RAM: %lx-%lx type=%u\r\n",
               d->PhysicalStart, d->PhysicalStart + d->NumberOfPages * 4096, d->Type);
        ConOut->OutputString(ConOut, buf);

        mmap_count++;
        if (mmap_count >= BOOTINFO_MAX_MMAP) break;
    }
    info->mmap = mmap;
    info->mmap_entries = mmap_count;

    // --- 3. Framebuffer info ---
    EFI_GUID gop_guid = {0x9042a9de,0x23dc,0x4a38,{0x96,0xfb,0x7a,0xde,0xd0,0x80,0x51,0x6a}};
    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop;
    status = BS->LocateProtocol(&gop_guid, NULL, (VOID**)&gop);
    if (status == EFI_SUCCESS) {
        bootinfo_framebuffer_t *fb = NULL;
        BS->AllocatePages(EFI_ALLOCATE_ANY_PAGES, EfiLoaderData, 1, (EFI_PHYSICAL_ADDRESS*)&fb);
        fb->address = gop->Mode->FrameBufferBase;
        fb->width   = gop->Mode->Info->HorizontalResolution;
        fb->height  = gop->Mode->Info->VerticalResolution;
        fb->pitch   = gop->Mode->Info->PixelsPerScanLine * 4;
        fb->bpp     = 32;
        fb->type    = 0;
        info->framebuffer = fb;

        // Fill the screen for demo
        UINT32 *pixels = (UINT32*)(UINTN)fb->address;
        for (UINT32 y = 0; y < fb->height; ++y)
            for (UINT32 x = 0; x < fb->width; ++x)
                pixels[y * (fb->pitch / 4) + x] = 0x00336699; // Dark blue
    } else {
        info->framebuffer = 0;
    }

    // --- 4. ACPI RSDP ---
    EFI_GUID acpi2_guid = {0x8868e871,0xe4f1,0x11d3,{0xbc,0x22,0x00,0x80,0xc7,0x3c,0x88,0x81}};
    VOID *rsdp = 0;
    status = BS->LocateProtocol(&acpi2_guid, NULL, &rsdp);
    info->acpi_rsdp = (uint64_t)rsdp;

    // --- 5. SMP (Not handled here, filled by kernel's ACPI code) ---
    info->cpu_count = 0;

    // --- 6. Read ELF kernel and load segments ---
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *FileSystem;
    status = BS->HandleProtocol(ImageHandle, (EFI_GUID*)&gEfiSimpleFileSystemProtocolGuid, (VOID**)&FileSystem);
    EFI_FILE_PROTOCOL *Root;
    status = FileSystem->OpenVolume(FileSystem, &Root);
    EFI_FILE_PROTOCOL *KernelFile;
    status = Root->Open(Root, &KernelFile, KERNEL_PATH, EFI_FILE_MODE_READ, 0);

    EFI_PHYSICAL_ADDRESS kernel_buf = 0x200000; // temp
    UINTN buf_pages = (KERNEL_MAX_SIZE + 4095) / 4096;
    status = BS->AllocatePages(EFI_ALLOCATE_ADDRESS, EfiLoaderData, buf_pages, &kernel_buf);
    UINTN kernel_size = KERNEL_MAX_SIZE;
    status = KernelFile->Read(KernelFile, &kernel_size, (VOID*)kernel_buf);
    KernelFile->Close(KernelFile);

    // Parse ELF and load all PT_LOAD segments
    Elf64_Ehdr *ehdr = (Elf64_Ehdr *)(UINTN)kernel_buf;
    Elf64_Phdr *phdrs = (Elf64_Phdr *)(UINTN)(kernel_buf + ehdr->e_phoff);
    for (uint16_t i = 0; i < ehdr->e_phnum; i++) {
        if (phdrs[i].p_type != 1) continue; // PT_LOAD
        UINT64 dest = phdrs[i].p_paddr;
        UINT64 src  = kernel_buf + phdrs[i].p_offset;
        UINTN seg_pages = (phdrs[i].p_memsz + 4095) / 4096;
        BS->AllocatePages(EFI_ALLOCATE_ADDRESS, EfiLoaderData, seg_pages, &dest);
        CopyMem((VOID *)(UINTN)dest, (VOID *)(UINTN)src, phdrs[i].p_filesz);
        if (phdrs[i].p_memsz > phdrs[i].p_filesz)
            SetMem((VOID *)(UINTN)(dest + phdrs[i].p_filesz), phdrs[i].p_memsz - phdrs[i].p_filesz, 0);
    }

    // --- 7. Final memory map and exit ---
    BS->GetMemoryMap(&mmap_size, efi_mmap, &map_key, &desc_size, &desc_ver);
    status = BS->ExitBootServices(ImageHandle, map_key);

    // --- 8. Jump to kernel entry (pass bootinfo ptr) ---
    void (*entry)(bootinfo_t*) = (void (*)(bootinfo_t*))(UINTN)ehdr->e_entry;
    entry(info);

    while (1);
    return EFI_SUCCESS;
}
