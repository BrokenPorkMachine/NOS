#include "efi.h"
#include "bootinfo.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define KERNEL_NAME L"\\kernel.bin"
#define MODULE_PREFIX L"module"
#define MODULE_SUFFIX L".bin"
#define MAX_MODULES 16
#define MAX_CPUS    256

#define FBINFO_MAGIC 0xF00DBA66
#define BOOTINFO_MAGIC_UEFI 0x4F324255

static void print_ascii(EFI_SYSTEM_TABLE *st, const char *s);
static EFI_STATUS load_file(EFI_SYSTEM_TABLE *st, EFI_FILE_PROTOCOL *root, const CHAR16 *path, void **buf, UINTN *size);

static uint64_t g_kernel_base = 0;
static uint64_t g_kernel_size = 0;

// --- Minimal C stdlib ---
static void *memcpy(void *dst, const void *src, size_t n) { uint8_t *d=dst; const uint8_t *s=src; while (n--) *d++ = *s++; return dst; }
static void *memset(void *dst, int c, size_t n) { uint8_t *d=dst; while (n--) *d++ = (uint8_t)c; return dst; }
static int memcmp(const void *a, const void *b, size_t n) { const uint8_t *x=a, *y=b; while (n--) { if (*x!=*y) return *x-*y; x++; y++; } return 0; }
static size_t strlen(const char *s) { size_t i=0; while(s[i]) ++i; return i; }
static void strcpy(char *dst, const char *src) { while((*dst++ = *src++)); }

// --- Print hex/dec ---
static void print_hex(EFI_SYSTEM_TABLE *st, uint64_t val) {
    char buf[19] = "0x0000000000000000";
    for (int i = 0; i < 16; ++i)
        buf[17 - i] = "0123456789ABCDEF"[(val >> (i * 4)) & 0xF];
    print_ascii(st, buf);
}
static void print_dec(EFI_SYSTEM_TABLE *st, uint64_t v) {
    char buf[22], *p = buf+21; *p = 0;
    if (!v) *--p = '0';
    while (v) { *--p = '0'+(v%10); v/=10; }
    print_ascii(st, p);
}
static void print_ascii(EFI_SYSTEM_TABLE *st, const char *s) {
    CHAR16 buf[256]; size_t i=0;
    while (i < 255 && s[i]) { buf[i] = (CHAR16)s[i]; i++; }
    buf[i]=0;
    st->ConOut->OutputString(st->ConOut, buf);
}

// --- GUIDs for system tables ---
static EFI_GUID acpi2_guid = {0x8868e871,0xe4f1,0x11d3,{0xbc,0x22,0x00,0x80,0xc7,0x3c,0x88,0x81}};
static EFI_GUID acpi_guid  = {0xeb9d2d30,0x2d88,0x11d3,{0x9a,0x16,0x00,0x90,0x27,0x3f,0xc1,0x4d}};
static EFI_GUID smbios3_guid = {0xf2fd1544,0x9794,0x4a2c,{0x99,0x2e,0xe5,0xbb,0xcf,0x20,0xe3,0x94}};
static EFI_GUID smbios_guid  = {0xeb9d2d31,0x2d88,0x11d3,{0x9a,0x16,0x00,0x90,0x27,0x3f,0xc1,0x4d}};

static void *find_uefi_config_table(EFI_SYSTEM_TABLE *st, EFI_GUID *guid) {
    for (UINTN i = 0; i < st->NumberOfTableEntries; ++i)
        if (!memcmp(&st->ConfigurationTable[i].VendorGuid, guid, sizeof(EFI_GUID)))
            return st->ConfigurationTable[i].VendorTable;
    return NULL;
}

// --- Bootinfo logger for diagnostics ---
static void log_bootinfo(EFI_SYSTEM_TABLE *st, const bootinfo_t *bi) {
    print_ascii(st, "[bootinfo] magic: "); print_hex(st, bi->magic); print_ascii(st, "\r\n");
    print_ascii(st, "[bootinfo] kernel: base="); print_hex(st, bi->kernel_load_base);
    print_ascii(st, " size="); print_hex(st, bi->kernel_load_size); print_ascii(st, "\r\n");
    print_ascii(st, "[bootinfo] kernel_entry: "); print_hex(st, (uint64_t)bi->kernel_entry); print_ascii(st, "\r\n");
    print_ascii(st, "[bootinfo] module_count: "); print_dec(st, bi->module_count); print_ascii(st, "\r\n");
    print_ascii(st, "[bootinfo] acpi_rsdp: "); print_hex(st, bi->acpi_rsdp); print_ascii(st, "\r\n");
    print_ascii(st, "[bootinfo] acpi_xsdt: "); print_hex(st, bi->acpi_xsdt); print_ascii(st, "\r\n");
    print_ascii(st, "[bootinfo] acpi_rsdt: "); print_hex(st, bi->acpi_rsdt); print_ascii(st, "\r\n");
    print_ascii(st, "[bootinfo] acpi_dsdt: "); print_hex(st, bi->acpi_dsdt); print_ascii(st, "\r\n");
    print_ascii(st, "[bootinfo] fb.addr: "); print_hex(st, bi->fb.address); print_ascii(st, " w="); print_dec(st, bi->fb.width); print_ascii(st, " h="); print_dec(st, bi->fb.height); print_ascii(st, "\r\n");
    print_ascii(st, "[bootinfo] cpu_count: "); print_dec(st, bi->cpu_count); print_ascii(st, "\r\n");
    print_ascii(st, "[bootinfo] mmap_entries: "); print_dec(st, bi->mmap_entries); print_ascii(st, "\r\n");
    print_ascii(st, "[bootinfo] smbios_entry: "); print_hex(st, bi->smbios_entry); print_ascii(st, "\r\n");
}

// --- Universal kernel type ---
typedef enum { KERNEL_TYPE_UNKNOWN=0, KERNEL_TYPE_ELF64, KERNEL_TYPE_MACHO64, KERNEL_TYPE_FLAT } kernel_type_t;

static kernel_type_t detect_kernel_type(const uint8_t *data, size_t size) {
    if (size >= 4 && data[0]==0x7F && data[1]=='E' && data[2]=='L' && data[3]=='F' && data[4]==2)
        return KERNEL_TYPE_ELF64;
    if (size >= 4 && ((data[0]==0xCF && data[1]==0xFA && data[2]==0xED && data[3]==0xFE) ||
                      (data[0]==0xFE && data[1]==0xED && data[2]==0xFA && data[3]==0xCF)))
        return KERNEL_TYPE_MACHO64;
    return KERNEL_TYPE_FLAT;
}

// --- ELF64 loader (PT_LOAD only, identity map) ---
static EFI_STATUS load_elf64(EFI_SYSTEM_TABLE *st, const void *image, UINTN size, void **entry) {
    typedef struct {
        unsigned char e_ident[16];
        UINT16  e_type, e_machine;
        UINT32  e_version;
        UINT64  e_entry, e_phoff, e_shoff;
        UINT32  e_flags;
        UINT16  e_ehsize, e_phentsize, e_phnum;
        UINT16  e_shentsize, e_shnum, e_shstrndx;
    } Elf64_Ehdr;
    typedef struct {
        UINT32  p_type, p_flags;
        UINT64  p_offset, p_vaddr, p_paddr, p_filesz, p_memsz, p_align;
    } Elf64_Phdr;

    if (size < sizeof(Elf64_Ehdr)) return EFI_LOAD_ERROR;
    const Elf64_Ehdr *eh = (const Elf64_Ehdr *)image;
    if (!(eh->e_ident[0]==0x7F && eh->e_ident[1]=='E' && eh->e_ident[2]=='L' && eh->e_ident[3]=='F'))
        return EFI_LOAD_ERROR;

    const Elf64_Phdr *ph = (const Elf64_Phdr *)((const UINT8 *)image + eh->e_phoff);
    UINT64 first = (UINT64)-1, last = 0;
    for (UINT16 i = 0; i < eh->e_phnum; ++i, ++ph) {
        if (ph->p_type != 1) continue; // PT_LOAD
        UINTN pages = (ph->p_memsz + 0xFFF) / 0x1000;
        EFI_PHYSICAL_ADDRESS seg = ph->p_paddr;
        EFI_STATUS s = st->BootServices->AllocatePages(EFI_ALLOCATE_ADDRESS, EfiLoaderData, pages, &seg);
        if (EFI_ERROR(s)) return s;
        memcpy((void *)(uintptr_t)seg, (const UINT8 *)image + ph->p_offset, ph->p_filesz);
        if (ph->p_memsz > ph->p_filesz)
            memset((void *)(uintptr_t)(seg + ph->p_filesz), 0, ph->p_memsz - ph->p_filesz);

        if (seg < first) first = seg;
        if (seg + ph->p_memsz > last) last = seg + ph->p_memsz;
    }

    g_kernel_base = first;
    g_kernel_size = last - first;
    *entry = (void *)(uintptr_t)eh->e_entry;
    return EFI_SUCCESS;
}

// --- Mach-O 64 (stub; implement as needed) ---
static EFI_STATUS load_macho64(EFI_SYSTEM_TABLE *st, const void *image, UINTN size, void **entry) {
    (void)st; (void)image; (void)size; (void)entry;
    print_ascii(st, "Mach-O loader not implemented yet\r\n");
    return EFI_LOAD_ERROR;
}

// --- Flat loader ---
static EFI_STATUS load_flat(EFI_SYSTEM_TABLE *st, const void *image, UINTN size, void **entry) {
    EFI_PHYSICAL_ADDRESS addr = 0x100000;
    UINTN pages = (size + 0xFFF) / 0x1000;
    EFI_STATUS s = st->BootServices->AllocatePages(EFI_ALLOCATE_ADDRESS, EfiLoaderData, pages, &addr);
    if (EFI_ERROR(s)) return s;
    memcpy((void *)(uintptr_t)addr, image, size);
    g_kernel_base = addr;
    g_kernel_size = size;
    *entry = (void *)(uintptr_t)addr;
    return EFI_SUCCESS;
}

// --- Load file from FS ---
static EFI_STATUS load_file(EFI_SYSTEM_TABLE *st, EFI_FILE_PROTOCOL *root,
                            const CHAR16 *path, void **buf, UINTN *size) {
    EFI_STATUS status;
    EFI_FILE_PROTOCOL *file;
    status = root->Open(root, &file, (CHAR16 *)path, 0x00000001, 0);
    if (EFI_ERROR(status)) return status;
    UINTN info_size = 0;
    status = file->GetInfo(file, (EFI_GUID *)&gEfiFileInfoGuid, &info_size, NULL);
    if (status != EFI_BUFFER_TOO_SMALL) { file->Close(file); return status; }
    EFI_FILE_INFO *info;
    status = st->BootServices->AllocatePool(EfiLoaderData, info_size, (void **)&info);
    if (EFI_ERROR(status)) { file->Close(file); return status; }
    status = file->GetInfo(file, (EFI_GUID *)&gEfiFileInfoGuid, &info_size, info);
    if (EFI_ERROR(status)) { st->BootServices->FreePool(info); file->Close(file); return status; }
    *size = info->FileSize;
    st->BootServices->FreePool(info);
    status = st->BootServices->AllocatePool(EfiLoaderData, *size, buf);
    if (EFI_ERROR(status)) { file->Close(file); return status; }
    UINTN to_read = *size;
    status = file->Read(file, &to_read, *buf);
    file->Close(file);
    return status;
}

// --- Scan/load modules: Any file with name "module*.bin" ---
static void scan_modules(EFI_SYSTEM_TABLE *SystemTable, EFI_FILE_PROTOCOL *root, bootinfo_t *bi) {
    EFI_FILE_PROTOCOL *dir = root;
    EFI_STATUS status;
    EFI_FILE_INFO *info;
    UINTN bufsz = 512 + sizeof(EFI_FILE_INFO);
    status = SystemTable->BootServices->AllocatePool(EfiLoaderData, bufsz, (void **)&info);
    if (EFI_ERROR(status)) return;
    UINTN n = 0;
    for (;;) {
        UINTN sz = bufsz;
        status = dir->Read(dir, &sz, info);
        if (EFI_ERROR(status) || sz == 0) break;
        if (!(info->Attribute & 0x10)) {
            CHAR16 *name = info->FileName;
            size_t i = 0;
            while (name[i] && MODULE_PREFIX[i] && name[i] == MODULE_PREFIX[i]) i++;
            if (MODULE_PREFIX[i] == 0 && name[i] >= '0' && name[i] <= '9') {
                size_t j = i+1, len = 0; while (name[len]) ++len;
                if (len > j+4 && name[len-4]=='.' && name[len-3]=='b' && name[len-2]=='i' && name[len-1]=='n') {
                    void *buf = NULL; UINTN fsz = 0;
                    status = load_file(SystemTable, root, name, &buf, &fsz);
                    if (!EFI_ERROR(status) && n < MAX_MODULES) {
                        bi->modules[n].base = buf;
                        bi->modules[n].size = fsz;
                        static char cname[64];
                        size_t k = 0;
                        while (name[k] && k < 63) { cname[k] = (char)name[k]; ++k; }
                        cname[k] = 0;
                        bi->modules[n].name = cname;
                        n++;
                    }
                }
            }
        }
    }
    bi->module_count = n;
    SystemTable->BootServices->FreePool(info);
}

// --- ACPI DSDT from RSDP/XSDT/RSDT ---
static uint64_t find_acpi_dsdt(uint64_t rsdp_addr) {
    if (!rsdp_addr) return 0;
    struct acpi_rsdp { char sig[8]; uint8_t chksum; char oemid[6]; uint8_t rev; uint32_t rsdt, len; uint64_t xsdt; } *rsdp = (void*)(uintptr_t)rsdp_addr;
    if (rsdp->rev >= 2 && rsdp->xsdt) {
        uint64_t *entries = (uint64_t *)((char *)(uintptr_t)rsdp->xsdt + 36);
        uint32_t count = (*(uint32_t *)((char *)(uintptr_t)rsdp->xsdt + 4) - 36) / 8;
        for (uint32_t i=0; i<count; i++) {
            char *h = (char *)(uintptr_t)entries[i];
            if (!memcmp(h, "FACP", 4)) return *(uint64_t *)(h + 40);
        }
    }
    if (rsdp->rsdt) {
        uint32_t *entries = (uint32_t *)((char *)(uintptr_t)rsdp->rsdt + 36);
        uint32_t count = (*(uint32_t *)((char *)(uintptr_t)rsdp->rsdt + 4) - 36) / 4;
        for (uint32_t i=0; i<count; i++) {
            char *h = (char *)(uintptr_t)entries[i];
            if (!memcmp(h, "FACP", 4)) return (uint64_t)*(uint32_t *)(h + 28);
        }
    }
    return 0;
}

// --- MADT parsing for LAPIC/CPU/IOAPIC ---
static void parse_madt(uint64_t madt_addr, bootinfo_t *bi) {
    if (!madt_addr) return;
    struct madt_hdr { char sig[4]; uint32_t len; uint8_t rev, cksum, oemid[6], oemtabid[8], oemrev[4], creator[4], creator_rev[4]; uint32_t lapic_addr; uint32_t flags; } __attribute__((packed));
    struct madt_hdr *hdr = (void*)(uintptr_t)madt_addr;
    bi->lapic_addr = hdr->lapic_addr;
    uint8_t *p = (uint8_t *)hdr + 44; uint8_t *end = (uint8_t *)hdr + hdr->len;
    uint32_t cpu_count = 0, ioapic_count = 0;
    while (p < end) {
        uint8_t type = p[0], len = p[1];
        if (type == 0 && len >= 8 && cpu_count < MAX_CPUS) { // Processor Local APIC
            uint8_t acpi_id = p[2], apic_id = p[3], flags = p[4];
            if (flags & 1) {
                bi->cpus[cpu_count].apic_id = apic_id;
                bi->cpus[cpu_count].acpi_id = acpi_id;
                bi->cpus[cpu_count].online = 1;
                bi->cpus[cpu_count].is_bsp = (cpu_count==0);
                cpu_count++;
            }
        }
        else if (type == 1 && len >= 12 && ioapic_count < 8) { // IOAPIC
            bi->ioapics[ioapic_count].ioapic_id = p[2];
            bi->ioapics[ioapic_count].ioapic_addr = *(uint32_t*)&p[4];
            bi->ioapics[ioapic_count].gsi_base   = *(uint32_t*)&p[8];
            ioapic_count++;
        }
        p += len;
    }
    bi->cpu_count = cpu_count;
    bi->ioapic_count = ioapic_count;
}

// --- Framebuffer info ---
typedef struct { uint32_t magic; void *base; uint32_t width, height, pitch, bpp; } fbinfo_t;
static EFI_STATUS find_framebuffer(EFI_SYSTEM_TABLE *st, fbinfo_t *fb) {
    static EFI_GUID gop_guid = {0x9042a9de,0x23dc,0x4a38,{0x96,0xfb,0x7a,0xde,0xd0,0x80,0x51,0x6a}};
    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop = NULL;
    EFI_STATUS s = st->BootServices->LocateProtocol(&gop_guid, NULL, (void **)&gop);
    if (EFI_ERROR(s)) return s;
    fb->magic = FBINFO_MAGIC;
    fb->base = (void *)(uintptr_t)gop->Mode->FrameBufferBase;
    fb->width = gop->Mode->Info->HorizontalResolution;
    fb->height = gop->Mode->Info->VerticalResolution;
    fb->pitch = gop->Mode->Info->PixelsPerScanLine * 4;
    fb->bpp = 32;
    return EFI_SUCCESS;
}

//
// --- MAIN ENTRY ---
//
EFI_STATUS EFIAPI efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    print_ascii(SystemTable, "\r\n[O2] Universal UEFI bootloader\r\n");

    // --- Filesystem and root directory ---
    EFI_LOADED_IMAGE_PROTOCOL *loaded;
    EFI_STATUS status = SystemTable->BootServices->HandleProtocol(ImageHandle,
        (EFI_GUID*)&gEfiLoadedImageProtocolGuid, (void**)&loaded);
    if (EFI_ERROR(status)) { print_ascii(SystemTable, "LoadProtocol failed\r\n"); return status; }
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs;
    status = SystemTable->BootServices->HandleProtocol(loaded->DeviceHandle,
        (EFI_GUID*)&gEfiSimpleFileSystemProtocolGuid, (void**)&fs);
    if (EFI_ERROR(status)) { print_ascii(SystemTable, "FS Protocol failed\r\n"); return status; }
    EFI_FILE_PROTOCOL *root;
    status = fs->OpenVolume(fs, &root);
    if (EFI_ERROR(status)) { print_ascii(SystemTable, "OpenVolume failed\r\n"); return status; }

    // --- Load kernel ---
    void *kernel_file = NULL; UINTN kernel_size = 0;
    status = load_file(SystemTable, root, KERNEL_NAME, &kernel_file, &kernel_size);
    if (EFI_ERROR(status)) { print_ascii(SystemTable, "Kernel not found\r\n"); return status; }
    kernel_type_t ktype = detect_kernel_type((const uint8_t*)kernel_file, kernel_size);
    print_ascii(SystemTable, "[O2] Kernel type: ");
    if (ktype == KERNEL_TYPE_ELF64) print_ascii(SystemTable, "ELF64\r\n");
    else if (ktype == KERNEL_TYPE_MACHO64) print_ascii(SystemTable, "Mach-O 64\r\n");
    else if (ktype == KERNEL_TYPE_FLAT) print_ascii(SystemTable, "Flat bin\r\n");
    else { print_ascii(SystemTable, "Unknown format!\r\n"); return EFI_LOAD_ERROR; }
    void *entry = NULL;
    if (ktype == KERNEL_TYPE_ELF64) status = load_elf64(SystemTable, kernel_file, kernel_size, &entry);
    else if (ktype == KERNEL_TYPE_MACHO64) status = load_macho64(SystemTable, kernel_file, kernel_size, &entry);
    else if (ktype == KERNEL_TYPE_FLAT) status = load_flat(SystemTable, kernel_file, kernel_size, &entry);
    if (EFI_ERROR(status)) { print_ascii(SystemTable, "Kernel load error\r\n"); return status; }
    print_ascii(SystemTable, "[O2] Kernel entry: "); print_hex(SystemTable, (uint64_t)(uintptr_t)entry); print_ascii(SystemTable, "\r\n");
    SystemTable->BootServices->FreePool(kernel_file);

    // --- Build bootinfo ---
    bootinfo_t *bi;
    EFI_PHYSICAL_ADDRESS bi_phys = 0;
    UINTN bi_pages = (sizeof(bootinfo_t) + 0xFFF) / 0x1000;
    status = SystemTable->BootServices->AllocatePages(EFI_ALLOCATE_ANY_PAGES, EfiLoaderData, bi_pages, &bi_phys);
    if (EFI_ERROR(status)) { print_ascii(SystemTable, "bootinfo alloc fail\r\n"); return status; }
    bi = (bootinfo_t *)(uintptr_t)bi_phys;
    memset(bi, 0, bi_pages * 0x1000);
    bi->magic = BOOTINFO_MAGIC_UEFI;
    bi->size = sizeof(*bi);

    // Bootloader name
    const char bl_name[] = "O2 UEFI";
    char *bl_copy = NULL;
    status = SystemTable->BootServices->AllocatePool(EfiLoaderData, sizeof(bl_name), (void **)&bl_copy);
    if (!EFI_ERROR(status)) { memcpy(bl_copy, bl_name, sizeof(bl_name)); bi->bootloader_name = bl_copy; }
    else { bi->bootloader_name = NULL; }

    bi->kernel_entry = entry;
    bi->kernel_load_base = g_kernel_base;
    bi->kernel_load_size = g_kernel_size;
    bi->cmdline = "";

    // --- ACPI tables ---
    void *rsdp = find_uefi_config_table(SystemTable, &acpi2_guid);
    if (!rsdp) rsdp = find_uefi_config_table(SystemTable, &acpi_guid);
    bi->acpi_rsdp = (uint64_t)(uintptr_t)rsdp;
    if (rsdp) {
        uint64_t xsdt = 0, rsdt = 0;
        if (((uint8_t*)rsdp)[15] >= 2) { xsdt = *(uint64_t *)((uint8_t*)rsdp + 24); bi->acpi_xsdt = xsdt; }
        rsdt = *(uint32_t *)((uint8_t*)rsdp + 16); bi->acpi_rsdt = rsdt;
        bi->acpi_dsdt = find_acpi_dsdt((uint64_t)(uintptr_t)rsdp);

        // --- Find MADT for LAPIC/cpu/ioapic ---
        uint64_t madt = 0;
        if (xsdt) {
            uint64_t *entries = (uint64_t *)((char *)(uintptr_t)xsdt + 36);
            uint32_t count = (*(uint32_t *)((char *)(uintptr_t)xsdt + 4) - 36) / 8;
            for (uint32_t i=0; i<count; i++) {
                char *h = (char *)(uintptr_t)entries[i];
                if (!memcmp(h, "APIC", 4)) madt = (uint64_t)(uintptr_t)h;
            }
        }
        if (!madt && rsdt) {
            uint32_t *entries = (uint32_t *)((char *)(uintptr_t)rsdt + 36);
            uint32_t count = (*(uint32_t *)((char *)(uintptr_t)rsdt + 4) - 36) / 4;
            for (uint32_t i=0; i<count; i++) {
                char *h = (char *)(uintptr_t)entries[i];
                if (!memcmp(h, "APIC", 4)) madt = (uint64_t)(uintptr_t)h;
            }
        }
        if (madt) parse_madt(madt, bi);
    }

    // --- SMBIOS (for hardware/BIOS info) ---
    void *smbios = find_uefi_config_table(SystemTable, &smbios3_guid);
    if (!smbios) smbios = find_uefi_config_table(SystemTable, &smbios_guid);
    bi->smbios_entry = (uint64_t)(uintptr_t)smbios;

    // --- RTC (UEFI time) ---
    EFI_TIME t;
    if (!SystemTable->RuntimeServices->GetTime(&t, NULL)) {
        bi->current_year   = t.Year;
        bi->current_month  = t.Month;
        bi->current_day    = t.Day;
        bi->current_hour   = t.Hour;
        bi->current_minute = t.Minute;
        bi->current_second = t.Second;
    }
    bi->uefi_system_table = SystemTable;

    // --- Framebuffer info
    fbinfo_t *fb;
    status = SystemTable->BootServices->AllocatePool(EfiLoaderData, sizeof(fbinfo_t), (void**)&fb);
    if (!EFI_ERROR(status)) {
        if (!EFI_ERROR(find_framebuffer(SystemTable, fb))) {
            bi->fb.address = (uint64_t)(uintptr_t)fb->base;
            bi->fb.width   = fb->width;
            bi->fb.height  = fb->height;
            bi->fb.pitch   = fb->pitch;
            bi->fb.bpp     = fb->bpp;
            bi->fb.type    = 0;
            bi->fb.reserved= 0;
        }
    }

    // --- Module scan/load ---
    scan_modules(SystemTable, root, bi);

    // --- Memory map ---
    UINTN mmapSize = 0, mapKey = 0, descSize = 0; UINT32 descVer = 0;
    SystemTable->BootServices->GetMemoryMap(&mmapSize, NULL, &mapKey, &descSize, &descVer);
    mmapSize += descSize * 2;
    status = SystemTable->BootServices->AllocatePool(EfiLoaderData, mmapSize, (void**)&bi->mmap);
    if (EFI_ERROR(status)) { print_ascii(SystemTable, "MMap alloc fail\r\n"); return status; }
    status = SystemTable->BootServices->GetMemoryMap(&mmapSize, bi->mmap, &mapKey, &descSize, &descVer);
    if (EFI_ERROR(status)) { print_ascii(SystemTable, "MMap read fail\r\n"); return status; }
    bi->mmap_entries = mmapSize / descSize;
    bi->mmap_desc_size = descSize;
    bi->mmap_desc_ver  = descVer;

    // --- Boot device info ---
    bi->boot_device_type = 0;
    bi->boot_partition = 0;

    // --- Log bootinfo before boot ---
    log_bootinfo(SystemTable, bi);

    // --- Exit Boot Services ---
    status = SystemTable->BootServices->ExitBootServices(ImageHandle, mapKey);
    if (EFI_ERROR(status)) { print_ascii(SystemTable, "ExitBootServices fail\r\n"); return status; }

    print_ascii(SystemTable, "[O2] Jumping to kernel...\r\n");
    void (*kernel_entry)(bootinfo_t*) = (void(*)(bootinfo_t*))bi->kernel_entry;
    kernel_entry(bi);
    return EFI_SUCCESS;
}
