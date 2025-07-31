#include "../include/efi.h"
#include "../include/bootinfo.h"
#include "kernel_loader.h"


#define KERNEL_PATH L"\\kernel.bin"
#define KERNEL_MAX_SIZE (2 * 1024 * 1024)

// --- Minimal Hex Printer for CHAR16 (prints 0x...64bit) ---
static void uefi_hex16(CHAR16 *buf, uint64_t val) {
    buf[0] = L'0'; buf[1] = L'x';
    int shift = 60;
    for (int i = 0; i < 16; ++i, shift -= 4)
        buf[2 + i] = L"0123456789ABCDEF"[(val >> shift) & 0xF];
    buf[18] = 0;
} 

static void print_hex(struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *ConOut, const CHAR16 *prefix, uint64_t val) {
    CHAR16 buf[64];
    int idx = 0;
    if (prefix) while (prefix[idx]) { buf[idx] = prefix[idx]; idx++; }
    buf[idx] = 0;
    uefi_hex16(buf + idx, val);
    for (int i = 0; buf[i]; ++i);
    buf[idx + 18] = L'\r'; buf[idx + 19] = L'\n'; buf[idx + 20] = 0;
    ConOut->OutputString(ConOut, buf);
}

// --- Minimal memcmp, memset ---
static int memcmp(const void *a, const void *b, unsigned n) {
    const unsigned char *p = a, *q = b;
    for (unsigned i = 0; i < n; ++i) if (p[i] != q[i]) return p[i] - q[i];
    return 0;
}
static void *memset(void *d, int v, unsigned n) {
    unsigned char *p = d; for (unsigned i = 0; i < n; ++i) p[i] = v; return d;
}

// --- Find ACPI RSDP from configuration tables ---
static VOID *find_acpi_rsdp(struct EFI_SYSTEM_TABLE *SystemTable) {
    EFI_CONFIGURATION_TABLE *ct = (EFI_CONFIGURATION_TABLE *)SystemTable->ConfigurationTable;
    for (UINTN i = 0; i < SystemTable->NumberOfTableEntries; ++i) {
        if (!memcmp(&ct[i].VendorGuid, &gEfiAcpi20TableGuid, sizeof(EFI_GUID)) ||
            !memcmp(&ct[i].VendorGuid, &gEfiAcpi10TableGuid, sizeof(EFI_GUID))) {
            return ct[i].VendorTable;
        }
    }
    return NULL;
}

// --- ELF structs ---
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

// --- Standalone ELF64 loader: returns 0 on success, nonzero on error ---
static int load_elf64_kernel(
    struct EFI_FILE_PROTOCOL *KernelFile,
    struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *ConOut,
    struct EFI_BOOT_SERVICES *BS,
    void (**entry_out)(bootinfo_t *))
{
    EFI_STATUS st;
    UINTN sz;
    Elf64_Ehdr eh;
    // Read ELF header
    sz = sizeof(eh);
    st = KernelFile->Read(KernelFile, &sz, &eh);
    if (st || sz != sizeof(eh)) { ConOut->OutputString(ConOut, L"ELF header read error\r\n"); return 1; }
    if (memcmp(eh.e_ident, "\x7f""ELF", 4) != 0 || eh.e_ident[4] != 2) {
        ConOut->OutputString(ConOut, L"Not ELF64\r\n"); return 2;
    }
    // Load segments
    for (UINTN i = 0; i < eh.e_phnum; ++i) {
        Elf64_Phdr ph;
        KernelFile->SetPosition(KernelFile, eh.e_phoff + i * sizeof(ph));
        sz = sizeof(ph);
        st = KernelFile->Read(KernelFile, &sz, &ph);
        if (st || sz != sizeof(ph)) { ConOut->OutputString(ConOut, L"PH read fail\r\n"); return 3; }
        if (ph.p_type != PT_LOAD || ph.p_memsz == 0) continue;
        print_hex(ConOut, L"Seg addr: ", ph.p_paddr);
        print_hex(ConOut, L"filesz: ", ph.p_filesz);
        print_hex(ConOut, L"memsz: ", ph.p_memsz);
        EFI_PHYSICAL_ADDRESS dest = ph.p_paddr;
        UINTN npages = (ph.p_memsz + 4095) / 4096;
        st = BS->AllocatePages(EFI_ALLOCATE_ADDRESS, EfiLoaderData, npages, &dest);
        if (st || dest != ph.p_paddr) { ConOut->OutputString(ConOut, L"Page alloc failed\r\n"); return 4; }
        KernelFile->SetPosition(KernelFile, ph.p_offset);
        sz = ph.p_filesz;
        st = KernelFile->Read(KernelFile, &sz, (void *)(UINTN)dest);
        if (st || sz != ph.p_filesz) { ConOut->OutputString(ConOut, L"Segment read fail\r\n"); return 5; }
        if (ph.p_memsz > ph.p_filesz)
            memset((void *)(UINTN)(dest + ph.p_filesz), 0, ph.p_memsz - ph.p_filesz);
    }
    *entry_out = (void (*)(bootinfo_t *))(UINTN)eh.e_entry;
    print_hex(ConOut, L"Kernel entry: ", eh.e_entry);
    return 0;
}

// -------------------- Main Bootloader Logic --------------------

EFI_STATUS efi_main(EFI_HANDLE ImageHandle, struct EFI_SYSTEM_TABLE *SystemTable) {
    EFI_STATUS status;
    struct EFI_BOOT_SERVICES *BS = SystemTable->BootServices;
    struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *ConOut = SystemTable->ConOut;
    ConOut->SetAttribute(ConOut, EFI_TEXT_ATTR(EFI_LIGHTGRAY, EFI_BLUE));
    ConOut->ClearScreen(ConOut);
    ConOut->OutputString(ConOut, L"NitrOBoot UEFI Loader starting...\r\n");
    ConOut->SetAttribute(ConOut, EFI_TEXT_ATTR(EFI_LIGHTGRAY, EFI_BLACK));

    // --- 1. Allocate and zero bootinfo struct ---
    bootinfo_t *info = NULL;
    status = BS->AllocatePages(EFI_ALLOCATE_ANY_PAGES, EfiLoaderData, 1, (EFI_PHYSICAL_ADDRESS*)&info);
    if (status != EFI_SUCCESS) { ConOut->OutputString(ConOut, L"Bootinfo alloc failed.\r\n"); for(;;); }
    memset(info, 0, sizeof(bootinfo_t));
    info->magic = BOOTINFO_MAGIC_UEFI;
    info->size = sizeof(bootinfo_t);
    info->bootloader_name = "NitrOBoot UEFI";
    print_hex(ConOut, L"bootinfo ptr: ", (UINTN)info);
    print_hex(ConOut, L"bootinfo size: ", info->size);
    print_hex(ConOut, L"bootinfo magic:", info->magic);
    print_hex(ConOut, L"bootinfo_memory_t size: ", sizeof(bootinfo_memory_t));

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

    UINTN desc_count = mmap_size / desc_size;

    bootinfo_memory_t *mmap = NULL;
    UINTN mmap_pages = (desc_count * sizeof(bootinfo_memory_t) + 4095) / 4096;
    status = BS->AllocatePages(EFI_ALLOCATE_ANY_PAGES, EfiLoaderData, mmap_pages,
                               (EFI_PHYSICAL_ADDRESS*)&mmap);
    if (status != EFI_SUCCESS) {
        ConOut->OutputString(ConOut, L"Bootinfo mmap alloc failed.\r\n");
        for(;;);
    }
    memset(mmap, 0, desc_count * sizeof(bootinfo_memory_t));

    UINTN mmap_count = 0;
    for (UINT8 *p = (UINT8*)efi_mmap; p < (UINT8*)efi_mmap + mmap_size; p += desc_size) {
        EFI_MEMORY_DESCRIPTOR *d = (EFI_MEMORY_DESCRIPTOR*)p;
        mmap[mmap_count].addr = d->PhysicalStart;
        mmap[mmap_count].len  = d->NumberOfPages * 4096;
        mmap[mmap_count].type = d->Type;
        mmap[mmap_count].reserved = 0;
        print_hex(ConOut, L"Mem Start: ", d->PhysicalStart);
        print_hex(ConOut, L"Mem End  : ", d->PhysicalStart + d->NumberOfPages * 4096);
        mmap_count++;
    }
    info->mmap = mmap;
    info->mmap_entries = (uint32_t)mmap_count;
    print_hex(ConOut, L"mmap struct ptr: ", (UINTN)mmap);
    print_hex(ConOut, L"mmap entries : ", mmap_count);

    // --- 3. Framebuffer info + fill screen ---
    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop = NULL;
    status = BS->LocateProtocol((EFI_GUID*)&gEfiGraphicsOutputProtocolGuid, NULL, (VOID**)&gop);
    if (status == EFI_SUCCESS && gop && gop->Mode) {
        bootinfo_framebuffer_t *fb = NULL;
        status = BS->AllocatePages(EFI_ALLOCATE_ANY_PAGES, EfiLoaderData, 1,
                                   (EFI_PHYSICAL_ADDRESS*)&fb);
        if (status == EFI_SUCCESS) {
            memset(fb, 0, sizeof(bootinfo_framebuffer_t));
            fb->address = gop->Mode->FrameBufferBase;
            fb->width   = gop->Mode->Info->HorizontalResolution;
            fb->height  = gop->Mode->Info->VerticalResolution;
            fb->pitch   = gop->Mode->Info->PixelsPerScanLine * 4;
            fb->bpp     = 32;
            fb->type    = 0;
            info->framebuffer = fb;
            print_hex(ConOut, L"fb struct ptr: ", (UINTN)fb);
            print_hex(ConOut, L"fb addr     : ", fb->address);
            print_hex(ConOut, L"fb width    : ", fb->width);
            print_hex(ConOut, L"fb height   : ", fb->height);
            print_hex(ConOut, L"fb pitch    : ", fb->pitch);
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
    VOID *rsdp = find_acpi_rsdp(SystemTable);
    info->acpi_rsdp = (uint64_t)rsdp;
    if (rsdp)
        ConOut->OutputString(ConOut, L"ACPI RSDP found and passed to kernel.\r\n");
    else
        ConOut->OutputString(ConOut, L"No ACPI RSDP found.\r\n");

    // --- 5. Kernel ELF load ---
    // Locate the simple filesystem protocol in the system
    struct EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *FileSystem;
    status = BS->LocateProtocol((EFI_GUID*)&gEfiSimpleFileSystemProtocolGuid,
                                NULL, (VOID**)&FileSystem);
    if (status != EFI_SUCCESS) { ConOut->OutputString(ConOut, L"FS protocol failed.\r\n"); for(;;); }
    struct EFI_FILE_PROTOCOL *Root;
    status = FileSystem->OpenVolume(FileSystem, &Root);
    if (status != EFI_SUCCESS) { ConOut->OutputString(ConOut, L"OpenVolume failed.\r\n"); for(;;); }
    struct EFI_FILE_PROTOCOL *KernelFile;
    status = Root->Open(Root, &KernelFile, KERNEL_PATH, EFI_FILE_MODE_READ, 0);
    if (status != EFI_SUCCESS) { ConOut->OutputString(ConOut, L"kernel.bin open failed.\r\n"); for(;;); }

    // --- 6. Load the ELF64 kernel ---
    void (*kernel_entry)(bootinfo_t*) = 0;
    int elf_status = load_elf64_kernel(KernelFile, ConOut, BS, &kernel_entry);
    KernelFile->Close(KernelFile);
    if (elf_status != 0 || !kernel_entry) { ConOut->OutputString(ConOut, L"Kernel load failed.\r\n"); for(;;); }

    // --- Allocate a stack for the kernel ---
    EFI_PHYSICAL_ADDRESS stack_base = 0;
    UINTN stack_pages = 8; // 32 KiB
    status = BS->AllocatePages(EFI_ALLOCATE_ANY_PAGES, EfiLoaderData,
                               stack_pages, &stack_base);
    if (status != EFI_SUCCESS) {
        ConOut->OutputString(ConOut, L"Kernel stack alloc failed.\r\n");
        for(;;);
    }
    memset((void *)(UINTN)stack_base, 0, stack_pages * 4096);
    UINT64 stack_top = stack_base + stack_pages * 4096ULL;
    void (*entry_fn)(bootinfo_t*) = kernel_entry;

    // Switch to the new stack before leaving boot services
    __asm__ __volatile__("mov %0, %%rsp" :: "r"(stack_top));

    // --- 7. Final memory map and ExitBootServices ---
    status = BS->GetMemoryMap(&mmap_size, efi_mmap, &map_key, &desc_size, &desc_ver);
    if (status == EFI_BUFFER_TOO_SMALL) {
        UINTN npages = (mmap_size + 4095) / 4096;
        status = BS->AllocatePages(EFI_ALLOCATE_ANY_PAGES, EfiLoaderData, npages,
                                  (EFI_PHYSICAL_ADDRESS*)&efi_mmap);
        if (status != EFI_SUCCESS) { ConOut->OutputString(ConOut, L"Memmap realloc failed.\r\n"); for(;;); }
        status = BS->GetMemoryMap(&mmap_size, efi_mmap, &map_key, &desc_size, &desc_ver);
    }
    if (status != EFI_SUCCESS) { ConOut->OutputString(ConOut, L"GetMemoryMap before ExitBootServices failed.\r\n"); for(;;); }
    status = BS->ExitBootServices(ImageHandle, map_key);
    if (status != EFI_SUCCESS) { ConOut->OutputString(ConOut, L"ExitBootServices failed.\r\n"); for(;;); }
    ConOut->OutputString(ConOut, L"Exiting boot services.\r\n");

    // --- 8. Handoff to kernel (bootinfo pointer) ---
    __asm__ __volatile__(
        "mov %0, %%rdi\n"
        "xor %%rbp, %%rbp\n"
        "jmp *%1\n"
        :
        : "r"(info), "r"(entry_fn)
        : "rdi", "rbp");

    for(;;);
    return EFI_SUCCESS;
}
