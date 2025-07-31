// src/NitrOBoot.c
#include "../include/efi.h"
#include "../include/bootinfo.h"

#define KERNEL_PATH L"\\EFI\\BOOT\\kernel.bin"
#define KERNEL_MAX_SIZE (2 * 1024 * 1024)
#define BOOTINFO_MAX_MMAP 128

// Minimal SPrint fallback (prints only 1 64-bit hex argument)
static void uefi_hex16(CHAR16 *buf, uint64_t val) {
    buf[0] = L'0'; buf[1] = L'x';
    int shift = 60;
    for (int i = 0; i < 16; ++i, shift -= 4)
        buf[2 + i] = L"0123456789ABCDEF"[(val >> shift) & 0xF];
    buf[18] = 0;
}

static void print_hex(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *ConOut, const CHAR16 *prefix, UINT64 val) {
    CHAR16 buf[64];
    int idx = 0;
    if (prefix) while (prefix[idx]) { buf[idx] = prefix[idx]; idx++; }
    buf[idx] = 0;
    uefi_hex16(buf + idx, val);
    for (int i = 0; buf[i]; ++i);
    buf[idx + 18] = L'\r'; buf[idx + 19] = L'\n'; buf[idx + 20] = 0;
    ConOut->OutputString(ConOut, buf);
}

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

// Minimal memory functions
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
    ConOut->SetAttribute(ConOut, EFI_TEXT_ATTR(EFI_LIGHTGRAY, EFI_BLUE));
    ConOut->ClearScreen(ConOut);
    ConOut->OutputString(ConOut, L"NitrOBoot UEFI Loader starting...\r\n");
    ConOut->SetAttribute(ConOut, EFI_TEXT_ATTR(EFI_LIGHTGRAY, EFI_BLACK));

    // --- 1. Allocate and zero bootinfo struct ---
    bootinfo_t *info = NULL;
    status = BS->AllocatePages(EFI_ALLOCATE_ANY_PAGES, EfiLoaderData, 1, (EFI_PHYSICAL_ADDRESS*)&info);
    if (status != EFI_SUCCESS) { ConOut->OutputString(ConOut, L"Bootinfo alloc failed.\r\n"); for(;;); }
    SetMem(info, sizeof(bootinfo_t), 0);
    info->magic = BOOTINFO_MAGIC_UEFI;
    info->size = sizeof(bootinfo_t);
    info->bootloader_name = "NitrOBoot UEFI";

    // --- 2. UEFI Memory map (copy & print) ---
    UINTN mmap_size = 0, map_key, desc_size;
    UINT32 desc_ver;
    status = BS->GetMemoryMap(&mmap_size, NULL, &map_key, &desc_size, &desc_ver);
    mmap_size += desc_size * 16;
    EFI_MEMORY_DESCRIPTOR *efi_mmap;
    status = BS->AllocatePages(EFI_ALLOCATE_ANY_PAGES, EfiLoaderData, (mmap_size + 4095) / 4096, (EFI_PHYSICAL_ADDRESS*)&efi_mmap);
    if (status != EFI_SUCCESS) { ConOut->OutputString(ConOut, L"Memmap alloc failed.\r\n"); for(;;); }
    status = BS->GetMemoryMap(&mmap_size, efi_mmap, &map_key, &desc_size, &desc_ver);
    if (status != EFI_SUCCESS) { ConOut->OutputString(ConOut, L"Memmap read failed.\r\n"); for(;;); }

    bootinfo_memory_t *mmap = NULL;
    status = BS->AllocatePages(EFI_ALLOCATE_ANY_PAGES, EfiLoaderData, 1, (EFI_PHYSICAL_ADDRESS*)&mmap);
    if (status != EFI_SUCCESS) { ConOut->OutputString(ConOut, L"Bootinfo mmap alloc failed.\r\n"); for(;;); }
    uint32_t mmap_count = 0;
    for (UINT8 *p = (UINT8*)efi_mmap; p < (UINT8*)efi_mmap + mmap_size; p += desc_size) {
        EFI_MEMORY_DESCRIPTOR *d = (EFI_MEMORY_DESCRIPTOR*)p;
        mmap[mmap_count].addr = d->PhysicalStart;
        mmap[mmap_count].len  = d->NumberOfPages * 4096;
        mmap[mmap_count].type = d->Type;
        mmap[mmap_count].reserved = 0;

        CHAR16 buf[96];
#if 0
        // If you have EDK2 SPrint, use this, else use hex printer above.
        SPrint(buf, sizeof(buf),
            L"RAM: %lx-%lx (%lu MiB) type=%u\r\n",
            d->PhysicalStart, d->PhysicalStart + d->NumberOfPages * 4096,
            mmap[mmap_count].len >> 20, d->Type);
        ConOut->OutputString(ConOut, buf);
#else
        print_hex(ConOut, L"Mem Start: ", d->PhysicalStart);
        print_hex(ConOut, L"Mem End  : ", d->PhysicalStart + d->NumberOfPages * 4096);
#endif

        mmap_count++;
        if (mmap_count >= BOOTINFO_MAX_MMAP) break;
    }
    info->mmap = mmap;
    info->mmap_entries = mmap_count;

    // --- 3. Framebuffer info + fill screen ---
    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop = NULL;
    status = BS->LocateProtocol((EFI_GUID*)&gEfiGraphicsOutputProtocolGuid, NULL, (VOID**)&gop);
    if (status == EFI_SUCCESS && gop && gop->Mode) {
        bootinfo_framebuffer_t *fb = NULL;
        status = BS->AllocatePages(EFI_ALLOCATE_ANY_PAGES, EfiLoaderData, 1, (EFI_PHYSICAL_ADDRESS*)&fb);
        if (status == EFI_SUCCESS) {
            fb->address = gop->Mode->FrameBufferBase;
            fb->width   = gop->Mode->Info->HorizontalResolution;
            fb->height  = gop->Mode->Info->VerticalResolution;
            fb->pitch   = gop->Mode->Info->PixelsPerScanLine * 4;
            fb->bpp     = 32;
            fb->type    = 0;
            info->framebuffer = fb;

            // Fill framebuffer with color
            UINT32 *pixels = (UINT32*)(UINTN)fb->address;
            for (UINT32 y = 0; y < fb->height; ++y)
                for (UINT32 x = 0; x < fb->width; ++x)
                    pixels[y * (fb->pitch / 4) + x] = 0x00336699; // Dark blue
            ConOut->OutputString(ConOut, L"Framebuffer filled with color.\r\n");
        }
    } else {
        info->framebuffer = 0;
        ConOut->OutputString(ConOut, L"Framebuffer NOT available.\r\n");
    }

    // --- 4. ACPI RSDP ---
    VOID *rsdp = 0;
    status = BS->LocateProtocol((EFI_GUID*)&gEfiAcpi20TableGuid, NULL, &rsdp);
    info->acpi_rsdp = (uint64_t)rsdp;
    if (rsdp)
        ConOut->OutputString(ConOut, L"ACPI RSDP found and passed to kernel.\r\n");
    else
        ConOut->OutputString(ConOut, L"No ACPI RSDP found.\r\n");

    // --- 5. Kernel ELF load ---
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *FileSystem;
    status = BS->HandleProtocol(ImageHandle, (EFI_GUID*)&gEfiSimpleFileSystemProtocolGuid, (VOID**)&FileSystem);
    if (status != EFI_SUCCESS) { ConOut->OutputString(ConOut, L"FS protocol failed.\r\n"); for(;;); }
    EFI_FILE_PROTOCOL *Root;
    status = FileSystem->OpenVolume(FileSystem, &Root);
    if (status != EFI_SUCCESS) { ConOut->OutputString(ConOut, L"OpenVolume failed.\r\n"); for(;;); }
    EFI_FILE_PROTOCOL *KernelFile;
    status = Root->Open(Root, &KernelFile, KERNEL_PATH, EFI_FILE_MODE_READ, 0);
    if (status != EFI_SUCCESS) { ConOut->OutputString(ConOut, L"kernel.bin open failed.\r\n"); for(;;); }

    EFI_PHYSICAL_ADDRESS kernel_buf = 0x200000;
    UINTN buf_pages = (KERNEL_MAX_SIZE + 4095) / 4096;
    status = BS->AllocatePages(EFI_ALLOCATE_ADDRESS, EfiLoaderData, buf_pages, &kernel_buf);
    if (status != EFI_SUCCESS) { ConOut->OutputString(ConOut, L"Kernel buffer alloc failed.\r\n"); for(;;); }
    UINTN kernel_size = KERNEL_MAX_SIZE;
    status = KernelFile->Read(KernelFile, &kernel_size, (VOID*)kernel_buf);
    KernelFile->Close(KernelFile);
    if (status != EFI_SUCCESS || kernel_size == 0) { ConOut->OutputString(ConOut, L"Read kernel.bin failed.\r\n"); for(;;); }

    // Parse ELF and load all PT_LOAD segments
    Elf64_Ehdr *ehdr = (Elf64_Ehdr *)(UINTN)kernel_buf;
    Elf64_Phdr *phdrs = (Elf64_Phdr *)(UINTN)(kernel_buf + ehdr->e_phoff);
    for (uint16_t i = 0; i < ehdr->e_phnum; i++) {
        if (phdrs[i].p_type != 1) continue; // PT_LOAD
        UINT64 dest = phdrs[i].p_paddr;
        UINT64 src  = kernel_buf + phdrs[i].p_offset;
        UINTN seg_pages = (phdrs[i].p_memsz + 4095) / 4096;
        status = BS->AllocatePages(EFI_ALLOCATE_ADDRESS, EfiLoaderData, seg_pages, &dest);
        if (status != EFI_SUCCESS) { ConOut->OutputString(ConOut, L"Kernel segment alloc failed.\r\n"); for(;;); }
        CopyMem((VOID *)(UINTN)dest, (VOID *)(UINTN)src, phdrs[i].p_filesz);
        if (phdrs[i].p_memsz > phdrs[i].p_filesz)
            SetMem((VOID *)(UINTN)(dest + phdrs[i].p_filesz), phdrs[i].p_memsz - phdrs[i].p_filesz, 0);
    }

    // --- 6. Final memory map and ExitBootServices ---
    status = BS->GetMemoryMap(&mmap_size, efi_mmap, &map_key, &desc_size, &desc_ver);
    if (status != EFI_SUCCESS) { ConOut->OutputString(ConOut, L"GetMemoryMap before ExitBootServices failed.\r\n"); for(;;); }
    status = BS->ExitBootServices(ImageHandle, map_key);
    if (status != EFI_SUCCESS) { ConOut->OutputString(ConOut, L"ExitBootServices failed.\r\n"); for(;;); }
    ConOut->OutputString(ConOut, L"Exiting boot services.\r\n");

    // --- 7. Handoff to kernel (bootinfo pointer) ---
    void (*entry)(bootinfo_t*) = (void (*)(bootinfo_t*))(UINTN)ehdr->e_entry;
    entry(info);

    for(;;);
    return EFI_SUCCESS;
}
