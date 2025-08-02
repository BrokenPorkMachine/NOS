#include "../include/efi.h"
#include "../include/bootinfo.h"
#include "kernel_loader.h"

static bootinfo_t *g_info = NULL;
static void (*g_entry)(bootinfo_t *) = NULL;


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
    buf[idx + 18] = L'\r';
    buf[idx + 19] = L'\n';
    buf[idx + 20] = 0;
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

// --- Validate ACPI RSDP structure ---
static int rsdp_valid(const VOID *rsdp) {
    const unsigned char *p = (const unsigned char *)rsdp;
    if (!p) return 0;
    if (memcmp(p, "RSD PTR ", 8) != 0) return 0;
    unsigned sum = 0;
    for (int i = 0; i < 20; ++i) sum += p[i];
    if ((sum & 0xFF) != 0) return 0;
    if (p[15] >= 2) {
        unsigned len = *(const uint32_t *)(p + 20);
        sum = 0;
        for (unsigned i = 0; i < len; ++i) sum += p[i];
        if ((sum & 0xFF) != 0) return 0;
    }
    return 1;
}

// --- Find ACPI RSDP from configuration tables ---
static VOID *find_acpi_rsdp(struct EFI_SYSTEM_TABLE *SystemTable) {
    EFI_CONFIGURATION_TABLE *ct = (EFI_CONFIGURATION_TABLE *)SystemTable->ConfigurationTable;
    for (UINTN i = 0; i < SystemTable->NumberOfTableEntries; ++i) {
        if (!memcmp(&ct[i].VendorGuid, &gEfiAcpi20TableGuid, sizeof(EFI_GUID)) ||
            !memcmp(&ct[i].VendorGuid, &gEfiAcpi10TableGuid, sizeof(EFI_GUID))) {
            VOID *rsdp = ct[i].VendorTable;
            if (rsdp_valid(rsdp))
                return rsdp;
        }
    }
    return NULL;
}

// --- Minimal CPUID helpers for basic CPU info ---
static inline void cpuid(UINT32 eax_in, UINT32 ecx_in,
                         UINT32 *eax, UINT32 *ebx,
                         UINT32 *ecx_out, UINT32 *edx)
{
    __asm__ __volatile__("cpuid"
                         : "=a"(*eax), "=b"(*ebx), "=c"(*ecx_out), "=d"(*edx)
                         : "a"(eax_in), "c"(ecx_in));
}

static UINT32 detect_logical_cpus(void)
{
    UINT32 max_leaf, eax, ebx, ecx, edx;
    cpuid(0, 0, &max_leaf, &ebx, &ecx, &edx);
    if (max_leaf >= 0x0B) {
        cpuid(0x0B, 0, &eax, &ebx, &ecx, &edx);
        UINT32 count = ebx & 0xFFFF;
        if (count == 0) count = 1;
        return count;
    }
    if (max_leaf >= 1) {
        cpuid(1, 0, &eax, &ebx, &ecx, &edx);
        UINT32 count = (ebx >> 16) & 0xFF;
        if (count == 0) count = 1;
        return count;
    }
    return 1;
}

static UINT32 detect_bsp_apic_id(void)
{
    UINT32 eax, ebx, ecx, edx;
    cpuid(1, 0, &eax, &ebx, &ecx, &edx);
    return (ebx >> 24) & 0xFF;
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
        ConOut->OutputString(ConOut, L"Not ELF64\r\n");
        return 2;
    }
    if (eh.e_machine != 0x3E) {
        ConOut->OutputString(ConOut, L"Unsupported arch\r\n");
        return 2;
    }
    if (eh.e_phentsize != sizeof(Elf64_Phdr) || eh.e_phnum == 0) {
        ConOut->OutputString(ConOut, L"Bad program header table\r\n");
        return 2;
    }
    // Load segments
    if (eh.e_phnum > 64) {
        ConOut->OutputString(ConOut, L"Too many segments\r\n");
        return 2;
    }
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
    ConOut->OutputString(ConOut, L"[Stage 1] Allocate bootinfo\r\n");

    // --- 1. Allocate and zero bootinfo struct ---
    bootinfo_t *info = NULL;
    EFI_PHYSICAL_ADDRESS info_addr = 0xFFFFFFFF;
    status = BS->AllocatePages(EFI_ALLOCATE_MAX_ADDRESS, EfiLoaderData, 1,
                               &info_addr);
    info = (bootinfo_t *)(UINTN)info_addr;
    if (status != EFI_SUCCESS) { ConOut->OutputString(ConOut, L"Bootinfo alloc failed.\r\n"); for(;;); }
    memset(info, 0, sizeof(bootinfo_t));
    info->magic = BOOTINFO_MAGIC_UEFI;
    info->size = sizeof(bootinfo_t);
    info->bootloader_name = "NitrOBoot UEFI";

    // Detect basic CPU information for the kernel
    info->cpu_count = detect_logical_cpus();
    if (info->cpu_count > 0) {
        info->cpus[0].processor_id = 0;
        info->cpus[0].apic_id = detect_bsp_apic_id();
        info->cpus[0].flags = 1;
    }
    print_hex(ConOut, L"cpu count: ", info->cpu_count);
    print_hex(ConOut, L"BSP APIC ID: ", info->cpus[0].apic_id);

    // Retrieve optional command line from UEFI load options
    EFI_LOADED_IMAGE_PROTOCOL *LoadedImage = NULL;
    status = BS->HandleProtocol(ImageHandle, (EFI_GUID*)&gEfiLoadedImageProtocolGuid,
                                (VOID**)&LoadedImage);
    if (status == EFI_SUCCESS && LoadedImage &&
        LoadedImage->LoadOptions && LoadedImage->LoadOptionsSize > 0) {
        UINTN opt_chars = LoadedImage->LoadOptionsSize / sizeof(CHAR16);
        EFI_PHYSICAL_ADDRESS cmd_addr = 0xFFFFFFFF;
        UINTN cmd_pages = (opt_chars + 1 + 4095) / 4096;
        if (BS->AllocatePages(EFI_ALLOCATE_MAX_ADDRESS, EfiLoaderData,
                              cmd_pages, &cmd_addr) == EFI_SUCCESS) {
            CHAR16 *src = (CHAR16*)LoadedImage->LoadOptions;
            char *dst = (char*)(UINTN)cmd_addr;
            UINTN i = 0;
            for (; i < opt_chars && src[i]; ++i)
                dst[i] = (char)(src[i] & 0xFF);
            dst[i] = 0;
            info->cmdline = dst;
        }
    }
    print_hex(ConOut, L"bootinfo ptr: ", (UINTN)info);
    print_hex(ConOut, L"bootinfo size: ", info->size);
    print_hex(ConOut, L"bootinfo magic:", info->magic);
    print_hex(ConOut, L"bootinfo_memory_t size: ", sizeof(bootinfo_memory_t));

    ConOut->OutputString(ConOut, L"[Stage 2] Retrieve memory map\r\n");
    // --- 2. UEFI Memory map (copy & print) ---
    UINTN mmap_size = 0, map_key, desc_size;
    UINT32 desc_ver;
    status = BS->GetMemoryMap(&mmap_size, NULL, &map_key, &desc_size, &desc_ver);
    if (desc_size < sizeof(EFI_MEMORY_DESCRIPTOR)) {
        ConOut->OutputString(ConOut, L"Memmap descriptor too small\r\n");
        for(;;);
    }
    mmap_size += desc_size * 64; // add initial slack for allocations
    EFI_MEMORY_DESCRIPTOR *efi_mmap = NULL;
    UINTN efi_mmap_pages = (mmap_size + 4095) / 4096;
    EFI_PHYSICAL_ADDRESS efi_mmap_addr = 0xFFFFFFFF;
    status = BS->AllocatePages(EFI_ALLOCATE_MAX_ADDRESS, EfiLoaderData,
                               efi_mmap_pages, &efi_mmap_addr);
    efi_mmap = (EFI_MEMORY_DESCRIPTOR *)(UINTN)efi_mmap_addr;
    if (status != EFI_SUCCESS) { ConOut->OutputString(ConOut, L"Memmap alloc failed.\r\n"); for(;;); }
    mmap_size = efi_mmap_pages * 4096;
    while (1) {
        status = BS->GetMemoryMap(&mmap_size, efi_mmap, &map_key, &desc_size, &desc_ver);
        if (status != EFI_BUFFER_TOO_SMALL)
            break;
        // allocate more space and retry
        efi_mmap_pages = (mmap_size + desc_size * 64 + 4095) / 4096;
        efi_mmap_addr = 0xFFFFFFFF;
        status = BS->AllocatePages(EFI_ALLOCATE_MAX_ADDRESS, EfiLoaderData,
                                   efi_mmap_pages, &efi_mmap_addr);
        efi_mmap = (EFI_MEMORY_DESCRIPTOR *)(UINTN)efi_mmap_addr;
        if (status != EFI_SUCCESS) { ConOut->OutputString(ConOut, L"Memmap resize failed.\r\n"); for(;;); }
        mmap_size = efi_mmap_pages * 4096;
    }
    if (status != EFI_SUCCESS) { ConOut->OutputString(ConOut, L"Memmap read failed.\r\n"); for(;;); }

    UINTN desc_count = mmap_size / desc_size;
    // Reserve extra space for final allocations made before ExitBootServices
    // but clamp to BOOTINFO_MAX_MMAP so the kernel never sees an overly
    // large map and halts.
    if (desc_count > BOOTINFO_MAX_MMAP)
        desc_count = BOOTINFO_MAX_MMAP;
    UINTN reserved_entries = desc_count + 64;
    if (reserved_entries > BOOTINFO_MAX_MMAP)
        reserved_entries = BOOTINFO_MAX_MMAP;

    bootinfo_memory_t *mmap = NULL;
    UINTN mmap_pages = (reserved_entries * sizeof(bootinfo_memory_t) + 4095) / 4096;
    EFI_PHYSICAL_ADDRESS mmap_addr = 0xFFFFFFFF;
    status = BS->AllocatePages(EFI_ALLOCATE_MAX_ADDRESS, EfiLoaderData,
                               mmap_pages, &mmap_addr);
    mmap = (bootinfo_memory_t *)(UINTN)mmap_addr;
    if (status != EFI_SUCCESS) {
        ConOut->OutputString(ConOut, L"Bootinfo mmap alloc failed.\r\n");
        for(;;);
    }
    memset(mmap, 0, reserved_entries * sizeof(bootinfo_memory_t));

    UINTN mmap_count = 0;
    for (UINT8 *p = (UINT8*)efi_mmap; p < (UINT8*)efi_mmap + mmap_size &&
                       mmap_count < reserved_entries; p += desc_size) {
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

    ConOut->OutputString(ConOut, L"[Stage 3] Framebuffer setup\r\n");
    // --- 3. Framebuffer info + fill screen ---
    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop = NULL;
    status = BS->LocateProtocol((EFI_GUID*)&gEfiGraphicsOutputProtocolGuid, NULL, (VOID**)&gop);
    if (status == EFI_SUCCESS && gop && gop->Mode) {
        bootinfo_framebuffer_t *fb = NULL;
        EFI_PHYSICAL_ADDRESS fb_addr = 0xFFFFFFFF;
        status = BS->AllocatePages(EFI_ALLOCATE_MAX_ADDRESS, EfiLoaderData, 1,
                                   &fb_addr);
        fb = (bootinfo_framebuffer_t *)(UINTN)fb_addr;
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

    ConOut->OutputString(ConOut, L"[Stage 4] ACPI lookup\r\n");
    // --- 4. ACPI RSDP ---
    VOID *rsdp = find_acpi_rsdp(SystemTable);
    info->acpi_rsdp = (uint64_t)rsdp;
    if (rsdp)
        ConOut->OutputString(ConOut, L"ACPI RSDP found and passed to kernel.\r\n");
    else
        ConOut->OutputString(ConOut, L"No ACPI RSDP found.\r\n");

    ConOut->OutputString(ConOut, L"[Stage 5] Load kernel image\r\n");
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

    UINT8 info_buf[sizeof(EFI_FILE_INFO) + 64];
    UINTN info_sz = sizeof(info_buf);
    EFI_STATUS info_st = KernelFile->GetInfo(KernelFile, (EFI_GUID*)&gEfiFileInfoGuid, &info_sz, info_buf);
    if (info_st != EFI_SUCCESS || info_sz < sizeof(EFI_FILE_INFO)) {
        ConOut->OutputString(ConOut, L"GetInfo failed.\r\n"); for(;;);
    }
    EFI_FILE_INFO *finfo = (EFI_FILE_INFO*)info_buf;
    if (finfo->FileSize > KERNEL_MAX_SIZE) {
        ConOut->OutputString(ConOut, L"Kernel too large.\r\n"); for(;;);
    }
    print_hex(ConOut, L"kernel size: ", finfo->FileSize);

    // --- 6. Load the ELF64 kernel ---
    void (*kernel_entry)(bootinfo_t*) = 0;
    int elf_status = load_elf64_kernel(KernelFile, ConOut, BS, &kernel_entry);
    KernelFile->Close(KernelFile);
    if (elf_status != 0 || !kernel_entry) { ConOut->OutputString(ConOut, L"Kernel load failed.\r\n"); for(;;); }
    info->kernel_entry = (void *)(UINTN)kernel_entry;
    print_hex(ConOut, L"kernel entry: ", (UINTN)kernel_entry);

    // --- Allocate a stack for the kernel ---
    EFI_PHYSICAL_ADDRESS stack_base = 0xFFFFFFFF;
    UINTN stack_pages = 8; // 32 KiB
    status = BS->AllocatePages(EFI_ALLOCATE_MAX_ADDRESS, EfiLoaderData,
                               stack_pages, &stack_base);
    if (status != EFI_SUCCESS) {
        ConOut->OutputString(ConOut, L"Kernel stack alloc failed.\r\n");
        for(;;);
    }
    memset((void *)(UINTN)stack_base, 0, stack_pages * 4096);
    UINT64 stack_top = stack_base + stack_pages * 4096ULL;
    void (*entry_fn)(bootinfo_t*) = kernel_entry;

    ConOut->OutputString(ConOut, L"[Stage 6] Exit boot services\r\n");
    // --- 7. Final memory map and ExitBootServices ---
    while (1) {
        status = BS->GetMemoryMap(&mmap_size, efi_mmap, &map_key, &desc_size, &desc_ver);
        if (status == EFI_BUFFER_TOO_SMALL) {
            // Allocate a larger buffer and retry
            efi_mmap_pages = (mmap_size + desc_size * 64 + 4095) / 4096;
            efi_mmap_addr = 0xFFFFFFFF;
            status = BS->AllocatePages(EFI_ALLOCATE_MAX_ADDRESS, EfiLoaderData,
                                       efi_mmap_pages, &efi_mmap_addr);
            efi_mmap = (EFI_MEMORY_DESCRIPTOR *)(UINTN)efi_mmap_addr;
            if (status != EFI_SUCCESS) {
                ConOut->OutputString(ConOut, L"Final memmap alloc failed.\r\n");
                for(;;);
            }
            mmap_size = efi_mmap_pages * 4096;
            continue;
        }
        if (status != EFI_SUCCESS) {
            ConOut->OutputString(ConOut, L"GetMemoryMap before ExitBootServices failed.\r\n");
            for(;;);
        }
        /* Do not call any Boot Services (including console output)
         * between GetMemoryMap and ExitBootServices. Printing here can
         * allocate memory and change the map, causing ExitBootServices
         * to fail with EFI_INVALID_PARAMETER. */
        status = BS->ExitBootServices(ImageHandle, map_key);
        if (status == EFI_SUCCESS)
            break;
    }

    // Copy final memory map to bootinfo (it may have changed)
    UINTN desc_count_final_total = mmap_size / desc_size;
    UINTN desc_count_final = desc_count_final_total;
    if (desc_count_final > reserved_entries)
        desc_count_final = reserved_entries;
    info->reserved[0] = desc_count_final_total;
    memset(info->mmap, 0, desc_count_final * sizeof(bootinfo_memory_t));
    for (UINTN i = 0; i < desc_count_final; ++i) {
        EFI_MEMORY_DESCRIPTOR *d = (EFI_MEMORY_DESCRIPTOR *)((UINT8 *)efi_mmap + i * desc_size);
        info->mmap[i].addr = d->PhysicalStart;
        info->mmap[i].len  = d->NumberOfPages * 4096;
        info->mmap[i].type = d->Type;
        info->mmap[i].reserved = 0;
    }
    info->mmap_entries = (uint32_t)desc_count_final;

    g_info = info;
    g_entry = entry_fn;

    // After ExitBootServices() we must avoid all boot service calls.
    // In particular the UEFI text console is no longer guaranteed
    // to be available, so skip the final status print.
    // --- 8. Switch stack and handoff to kernel ---
    __asm__ __volatile__(
        "mov %0, %%rsp\n"
        "mov %1, %%rdi\n"
        "xor %%rbp, %%rbp\n"
        "jmp *%2\n"
        :
        : "r"(stack_top), "r"(g_info), "r"(g_entry)
        : "rsp", "rdi", "rbp");

    for(;;);
    return EFI_SUCCESS;
}
