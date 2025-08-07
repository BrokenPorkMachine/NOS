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
#define MAX_KERNEL_SEGMENTS 16
#define FBINFO_MAGIC 0xF00DBA66
#define BOOTINFO_MAGIC_UEFI 0x4F324255

static void *memcpy(void *dst, const void *src, size_t n) { uint8_t *d=dst; const uint8_t *s=src; while (n--) *d++ = *s++; return dst; }
static void *memset(void *dst, int c, size_t n) { uint8_t *d=dst; while (n--) *d++ = (uint8_t)c; return dst; }
static int memcmp(const void *a, const void *b, size_t n) { const uint8_t *x=a, *y=b; while (n--) { if (*x!=*y) return *x-*y; x++; y++; } return 0; }
static size_t strlen(const char *s) { size_t i=0; while(s[i]) ++i; return i; }
static void strcpy(char *dst, const char *src) { while((*dst++ = *src++)); }

static uint64_t g_kernel_base = 0, g_kernel_size = 0;

static void print_ascii(EFI_SYSTEM_TABLE *st, const char *s) {
    CHAR16 buf[256]; size_t i=0;
    while (i < 255 && s[i]) { buf[i] = (CHAR16)s[i]; i++; }
    buf[i]=0;
    st->ConOut->OutputString(st->ConOut, buf);
}
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

// Update your bootinfo_t in bootinfo.h to include these fields:
// kernel_segment_t kernel_segments[MAX_KERNEL_SEGMENTS];
// uint32_t kernel_segment_count;

static void log_bootinfo(EFI_SYSTEM_TABLE *st, const bootinfo_t *bi) {
    print_ascii(st, "[bootinfo] magic: "); print_hex(st, bi->magic); print_ascii(st, "\r\n");
    print_ascii(st, "[bootinfo] kernel: base="); print_hex(st, bi->kernel_load_base);
    print_ascii(st, " size="); print_hex(st, bi->kernel_load_size); print_ascii(st, "\r\n");
    print_ascii(st, "[bootinfo] kernel_entry: "); print_hex(st, (uint64_t)bi->kernel_entry); print_ascii(st, "\r\n");
    print_ascii(st, "[bootinfo] kernel_segment_count: "); print_dec(st, bi->kernel_segment_count); print_ascii(st, "\r\n");
    for (uint32_t i=0; i < bi->kernel_segment_count; i++) {
        print_ascii(st, "  [seg] vaddr="); print_hex(st, bi->kernel_segments[i].vaddr);
        print_ascii(st, " paddr="); print_hex(st, bi->kernel_segments[i].paddr);
        print_ascii(st, " filesz="); print_hex(st, bi->kernel_segments[i].filesz);
        print_ascii(st, " memsz="); print_hex(st, bi->kernel_segments[i].memsz);
        print_ascii(st, " flags="); print_hex(st, bi->kernel_segments[i].flags);
        if (bi->kernel_segments[i].name[0]) {
            print_ascii(st, " name=");
            print_ascii(st, bi->kernel_segments[i].name);
        }
        print_ascii(st, "\r\n");
    }
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

// ... (kernel type detection, GUIDs, find_uefi_config_table, etc unchanged from earlier) ...

// --- ELF64 loader with segment debug ---
static EFI_STATUS load_elf64(EFI_SYSTEM_TABLE *st, const void *image, UINTN size, void **entry, kernel_segment_t *segs, uint32_t *segc) {
    typedef struct { unsigned char e_ident[16]; UINT16 e_type,e_machine; UINT32 e_version; UINT64 e_entry,e_phoff,e_shoff; UINT32 e_flags; UINT16 e_ehsize,e_phentsize,e_phnum; UINT16 e_shentsize,e_shnum,e_shstrndx; } Elf64_Ehdr;
    typedef struct { UINT32 p_type,p_flags; UINT64 p_offset,p_vaddr,p_paddr,p_filesz,p_memsz,p_align; } Elf64_Phdr;
    if (size < sizeof(Elf64_Ehdr)) return EFI_LOAD_ERROR;
    const Elf64_Ehdr *eh = (const Elf64_Ehdr *)image;
    if (!(eh->e_ident[0]==0x7F && eh->e_ident[1]=='E' && eh->e_ident[2]=='L' && eh->e_ident[3]=='F')) return EFI_LOAD_ERROR;
    const Elf64_Phdr *ph = (const Elf64_Phdr *)((const UINT8 *)image + eh->e_phoff);
    UINT64 first = (UINT64)-1, last = 0;
    uint32_t count = 0;
    for (UINT16 i = 0; i < eh->e_phnum; ++i, ++ph) {
        if (ph->p_type != 1) continue; // PT_LOAD
        UINTN pages = (ph->p_memsz + 0xFFF) / 0x1000;
        EFI_PHYSICAL_ADDRESS seg = ph->p_paddr;
        EFI_STATUS s = st->BootServices->AllocatePages(EFI_ALLOCATE_ADDRESS, EfiLoaderData, pages, &seg);
        if (EFI_ERROR(s)) return s;
        memcpy((void *)(uintptr_t)seg, (const UINT8 *)image + ph->p_offset, ph->p_filesz);
        if (ph->p_memsz > ph->p_filesz)
            memset((void *)(uintptr_t)(seg + ph->p_filesz), 0, ph->p_memsz - ph->p_filesz);
        // Debug output
        print_ascii(st, "[O2] ELF LOAD: vaddr="); print_hex(st, ph->p_vaddr);
        print_ascii(st, " paddr="); print_hex(st, seg);
        print_ascii(st, " filesz="); print_hex(st, ph->p_filesz);
        print_ascii(st, " memsz="); print_hex(st, ph->p_memsz);
        print_ascii(st, " flags="); print_hex(st, ph->p_flags); print_ascii(st, "\r\n");
        // Record segment
        if (segs && segc && count < MAX_KERNEL_SEGMENTS) {
            segs[count].vaddr  = ph->p_vaddr;
            segs[count].paddr  = seg;
            segs[count].filesz = ph->p_filesz;
            segs[count].memsz  = ph->p_memsz;
            segs[count].flags  = ph->p_flags;
            segs[count].name[0]=0;
            ++count;
        }
        if (seg < first) first = seg;
        if (seg + ph->p_memsz > last) last = seg + ph->p_memsz;
    }
    if (segc) *segc = count;
    g_kernel_base = first;
    g_kernel_size = last - first;
    *entry = (void *)(uintptr_t)eh->e_entry;
    return EFI_SUCCESS;
}

// --- Mach-O, FAT, PE/COFF, Flat loaders: each should do the same: print debug, fill segs[] ---
/* ... include your previous Mach-O and PE/COFF loader code, but inside their segment loading loops:
   - print segment info using print_ascii/print_hex
   - add info to segs[] as in the ELF loader above
   - increment *segc for each loaded segment
   - copy segment name for Mach-O as needed
   - see previous responses for unified loader code for Mach-O, FAT, PE, Flat ...
*/

// Example Mach-O segment loop (inside load_macho64, simplified):
/*
for each LC_SEGMENT_64:
    allocate/memcpy...
    print_ascii(st, "[O2] MACHO LOAD: vmaddr="); print_hex(st, seg->vmaddr); ...
    if (segs && segc && count < MAX_KERNEL_SEGMENTS) {
        segs[count].vaddr  = seg->vmaddr;
        segs[count].paddr  = seg_addr;
        segs[count].filesz = seg->filesize;
        segs[count].memsz  = seg->vmsize;
        segs[count].flags  = seg->initprot;
        memcpy(segs[count].name, seg->segname, 16); segs[count].name[16]=0;
        ++count;
    }
*/

//
// --- MAIN ENTRY ---
//
EFI_STATUS EFIAPI efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    print_ascii(SystemTable, "\r\n[O2] Universal UEFI bootloader\r\n");
    EFI_LOADED_IMAGE_PROTOCOL *loaded;
    EFI_STATUS status = SystemTable->BootServices->HandleProtocol(ImageHandle, (EFI_GUID*)&gEfiLoadedImageProtocolGuid, (void**)&loaded);
    if (EFI_ERROR(status)) { print_ascii(SystemTable, "LoadProtocol failed\r\n"); return status; }
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs;
    status = SystemTable->BootServices->HandleProtocol(loaded->DeviceHandle, (EFI_GUID*)&gEfiSimpleFileSystemProtocolGuid, (void**)&fs);
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
    // ... print type ...
    void *entry = NULL;
    kernel_segment_t kernel_segments[MAX_KERNEL_SEGMENTS];
    uint32_t kernel_segment_count = 0;
    if (ktype == KERNEL_TYPE_ELF64)
        status = load_elf64(SystemTable, kernel_file, kernel_size, &entry, kernel_segments, &kernel_segment_count);
    // ... Mach-O/PE/Flat loaders, pass kernel_segments, &kernel_segment_count ...
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
    // Copy kernel segment table
    memcpy(bi->kernel_segments, kernel_segments, sizeof(kernel_segment_t)*kernel_segment_count);
    bi->kernel_segment_count = kernel_segment_count;

    // ... rest of bootinfo as before ...
    // (copy in loader name, kernel entry, base, modules, ACPI, fb, etc)

    // --- Log bootinfo before boot ---
    log_bootinfo(SystemTable, bi);

    // --- Exit Boot Services and jump to kernel ---
    UINTN mmap_size = 0, map_key, desc_size;
    UINT32 desc_ver;
    EFI_MEMORY_DESCRIPTOR *mmap = NULL;

    status = SystemTable->BootServices->GetMemoryMap(&mmap_size, mmap, &map_key, &desc_size, &desc_ver);
    if (status == EFI_BUFFER_TOO_SMALL) {
        mmap_size += desc_size * 2;
        status = SystemTable->BootServices->AllocatePool(EfiLoaderData, mmap_size, (void**)&mmap);
        if (!EFI_ERROR(status))
            status = SystemTable->BootServices->GetMemoryMap(&mmap_size, mmap, &map_key, &desc_size, &desc_ver);
    }
    if (EFI_ERROR(status)) return status;

    bi->mmap = (bootinfo_memory_t *)mmap;
    bi->mmap_entries   = mmap_size / desc_size;
    bi->mmap_desc_size = desc_size;
    bi->mmap_desc_ver  = desc_ver;

    status = SystemTable->BootServices->ExitBootServices(ImageHandle, map_key);
    if (EFI_ERROR(status)) return status;

    void (*kentry)(bootinfo_t*) = (void(*)(bootinfo_t*))entry;
    kentry(bi);

    // If the kernel returns, halt
    for (;;) { __asm__ __volatile__("hlt"); }
}
