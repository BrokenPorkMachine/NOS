// src/NitrOBoot.c
#include "../include/efi.h"
#include "../include/bootinfo.h"

#define KERNEL_PATH L"\\EFI\\BOOT\\kernel.bin"
#define KERNEL_MAX_SIZE (2 * 1024 * 1024)
#define BOOTINFO_MAX_MMAP 128

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

// Simple memcpy/setmem for boot context
VOID *EFIAPI CopyMem(VOID *d, const VOID *s, UINTN l) {
    UINT8 *dst = (UINT8*)d; const UINT8 *src = (const UINT8*)s;
    for (UINTN i = 0; i < l; ++i) dst[i] = src[i];
    return d;
}
VOID *EFIAPI SetMem(VOID *b, UINTN l, UINT8 v) {
    UINT8 *buf = (UINT8*)b; for (UINTN i = 0; i < l; ++i) buf[i] = v; return b;
}

// SPrint for UEFI logging
static void PrintUefiHex(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *ConOut, const CHAR16 *prefix, UINT64 val) {
    static const CHAR16 digits[] = L"0123456789ABCDEF";
    CHAR16 buf[64]; int idx = 0;
    while (prefix && prefix[idx]) { buf[idx] = prefix[idx]; idx++; }
    buf[idx++] = L'0'; buf[idx++] = L'x';
    int sh = 60, start = idx;
    for (; sh >= 0; sh -= 4) buf[idx++] = digits[(val >> sh) & 0xF];
    buf[idx++] = L'\r'; buf[idx++] = L'\n'; buf[idx] = 0;
    ConOut->OutputString(ConOut, buf + start); // Skip prefix for alignment
}

// --------- UEFI Bootloader Main Entry ----------
EFI_STATUS efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    EFI_STATUS status;
    EFI_BOOT_SERVICES *BS = SystemTable->BootServices;
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *ConOut = SystemTable->ConOut;
    ConOut->SetAttribute(ConOut, EFI_TEXT_ATTR(EFI_LIGHTGRAY, EFI_BLUE));
    ConOut->ClearScreen(ConOut);
    ConOut->OutputString(ConOut, L"NitrOBoot UEFI Loader starting...\r\n");
    ConOut->SetAttribute(ConOut, EFI_TEXT_ATTR(EFI_LIGHTGRAY, EFI_BLACK));

    // --- 1. Bootinfo struct ---
    bootinfo_t *info = NULL;
    BS->AllocatePages(EFI_ALLOCATE_ANY_PAGES, EfiLoaderData, 1, (EFI_PHYSICAL_ADDRESS*)&info);
    SetMem(info, sizeof(bootinfo_t), 0);
    info->magic = BOOTINFO_MAGIC_UEFI;
    info->size = sizeof(bootinfo_t);
    info->bootloader_name = "NitrOBoot UEFI";

    // --- 2. Memory map & Print ---
    UINTN mmap_size = 0, map_key, desc_size;
    UINT32 desc_ver;
    BS->GetMemoryMap(&mmap_size, NULL, &map_key, &desc_size, &desc_ver);
    mmap_size += desc_size * 16;
    EFI_MEMORY_DESCRIPTOR *efi_mmap;
    BS->AllocatePages(EFI_ALLOCATE_ANY_PAGES, EfiLoaderData, (mmap_size + 4095) / 4096, (EFI_PHYSICAL_ADDRESS*)&efi_mmap);
    status = BS->GetMemoryMap(&mmap_size, efi_mmap, &map_key, &desc_size, &desc_ver);

    bootinfo_memory_t *mmap = NULL;
    BS->AllocatePages(EFI_ALLOCATE_ANY_PAGES, EfiLoaderData, 1, (EFI_PHYSICAL_ADDRESS*)&mmap);
    uint32_t mmap_count = 0;
    for (UINT8 *p = (UINT8*)efi_mmap; p < (UINT8*)efi_mmap + mmap_size; p += desc_size) {
        EFI_MEMORY_DESCRIPTOR *d = (EFI_MEMORY_DESCRIPTOR*)p;
        mmap[mmap_count].addr = d->PhysicalStart;
        mmap[mmap_count].len  = d->NumberOfPages * 4096;
        mmap[mmap_count].type = d->Type;
        mmap[mmap_count].reserved = 0;

        CHAR16 buf[96];
        // Print region, with size in MiB
        UINT64 size_mb = mmap[mmap_count].len >> 20;
        SPrint(buf, sizeof(buf),
            L"RAM: %lx-%lx (%llu MiB) type=%u\r\n",
            d->PhysicalStart, d->PhysicalStart + d->NumberOfPages * 4096,
            size_mb, d->Type);
        ConOut->OutputString(ConOut, buf);

        mmap_count++;
        if (mmap_count >= BOOTINFO_MAX_MMAP) break;
    }
    info->mmap = mmap;
    info->mmap_entries = mmap_count;

    // --- 3. Framebuffer info + fill screen ---
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

        // Fill framebuffer with pattern/color
        UINT32 *pixels = (UINT32*)(UINTN)fb->address;
        for (UINT32 y = 0; y < fb->height; ++y)
            for (UINT32 x = 0; x < fb->width; ++x)
                pixels[y * (fb->pitch / 4) + x] = 0x00336699; // Dark blue
        ConOut->OutputString(ConOut, L"Framebuffer filled with color.\r\n");
    } else {
        info->framebuffer = 0;
        ConOut->OutputString(ConOut, L"Framebuffer NOT available.\r\n");
    }

    // --- 4. ACPI RSDP ---
    EFI_GUID acpi2_guid = {0x8868e871,0xe4f1,0x11d3,{0xbc,0x22,0x00,0x80,0xc7,0x3c,0x88,0x81}};
    VOID *rsdp = 0;
    status = BS->LocateProtocol(&acpi2_guid, NULL, &rsdp);
    info->acpi_rsdp = (uint64_t)rsdp;
    if (rsdp)
        ConOut->OutputString(ConOut, L"ACPI RSDP found and passed to kernel.\r\n");
    else
        ConOut->OutputString(ConOut, L"No ACPI RSDP found.\r\n");

    // --- 5. Kernel ELF load ---
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
        status = BS->AllocatePages(EFI_ALLOCATE_ADDRESS, EfiLoaderData, seg_pages, &dest);
        CopyMem((VOID *)(UINTN)dest, (VOID *)(UINTN)src, phdrs[i].p_filesz);
        if (phdrs[i].p_memsz > phdrs[i].p_filesz)
            SetMem((VOID *)(UINTN)(dest + phdrs[i].p_filesz), phdrs[i].p_memsz - phdrs[i].p_filesz, 0);
    }

    // --- 6. Final memory map and exit boot services ---
    BS->GetMemoryMap(&mmap_size, efi_mmap, &map_key, &desc_size, &desc_ver);
    status = BS->ExitBootServices(ImageHandle, map_key);
    ConOut->OutputString(ConOut, L"Exiting boot services.\r\n");

    // --- 7. Handoff to kernel, pass bootinfo pointer ---
    void (*entry)(bootinfo_t*) = (void (*)(bootinfo_t*))(UINTN)ehdr->e_entry;
    entry(info);

    for(;;);
    return EFI_SUCCESS;
}
