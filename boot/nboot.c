#include "efi.h"
#include "bootinfo.h"
#include <stdint.h>
#include <stddef.h>

#define O2_NAME    L"\\O2.bin"
#define N2_NAME    L"\\n2.bin"
#define MAX_MODULES 16
#define FBINFO_MAGIC 0xF00DBA66
#define BOOTINFO_MAGIC_UEFI 0x4F324255

// --- Mini stdlib ---
static void *memcpy(void *dst, const void *src, size_t n) { uint8_t *d=dst; const uint8_t *s=src; while (n--) *d++ = *s++; return dst; }
static void *memset(void *dst, int c, size_t n) { uint8_t *d=dst; while (n--) *d++ = (uint8_t)c; return dst; }
static int memcmp(const void *a, const void *b, size_t n) { const uint8_t *x=a, *y=b; while (n--) { if (*x!=*y) return *x-*y; x++; y++; } return 0; }
static size_t strlen(const char *s) { size_t i=0; while(s[i]) ++i; return i; }
static void strcpy(char *dst, const char *src) { while((*dst++ = *src++)); }

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

// --- EFI File read helper ---
EFI_STATUS load_file(EFI_SYSTEM_TABLE *st, EFI_FILE_PROTOCOL *root,
                     const CHAR16 *path, UINTN mem_type,
                     void **buf, UINTN *size) {
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
    status = st->BootServices->AllocatePool(mem_type, *size, buf);
    if (EFI_ERROR(status)) { file->Close(file); return status; }
    UINTN to_read = *size;
    status = file->Read(file, &to_read, *buf);
    file->Close(file);
    return status;
}

// --- Framebuffer info ---
typedef struct {
    uint32_t magic;
    void *base;
    uint32_t width, height, pitch, bpp;
} fbinfo_t;
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

// --- MAIN ENTRY ---
EFI_STATUS EFIAPI efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    print_ascii(SystemTable, "\r\n[nboot] UEFI multi-stage loader\r\n");

    // --- Find FS root ---
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

    // --- Allocate bootinfo struct ---
    bootinfo_t *bi;
    EFI_PHYSICAL_ADDRESS bi_phys = 0;
    UINTN bi_pages = (sizeof(bootinfo_t) + 0xFFF) / 0x1000;
    status = SystemTable->BootServices->AllocatePages(EFI_ALLOCATE_ANY_PAGES, EfiLoaderData, bi_pages, &bi_phys);
    if (EFI_ERROR(status)) { print_ascii(SystemTable, "bootinfo alloc fail\r\n"); return status; }
    bi = (bootinfo_t *)(uintptr_t)bi_phys;
    memset(bi, 0, bi_pages * 0x1000);
    bi->magic = BOOTINFO_MAGIC_UEFI;
    bi->size = sizeof(*bi);

    // --- Load O2.bin as "kernel" ---
    void *o2_buf = NULL; UINTN o2_size = 0;
    status = load_file(SystemTable, root, O2_NAME, EfiLoaderCode, &o2_buf, &o2_size);
    if (EFI_ERROR(status)) { print_ascii(SystemTable, "O2.bin not found\r\n"); return status; }

    // --- Load n2.bin as module ---
    void *n2_buf = NULL; UINTN n2_size = 0;
    status = load_file(SystemTable, root, N2_NAME, EfiLoaderData, &n2_buf, &n2_size);
    if (EFI_ERROR(status)) { print_ascii(SystemTable, "n2.bin not found\r\n"); return status; }
    print_ascii(SystemTable, "[nboot] Loaded n2.bin: ");
    print_hex(SystemTable, (uint64_t)(uintptr_t)n2_buf); print_ascii(SystemTable, " sz=");
    print_hex(SystemTable, n2_size); print_ascii(SystemTable, "\r\n");

    // --- Fill in module slot ---
    bi->modules[0].name = "n2.bin";
    bi->modules[0].base = n2_buf;
    bi->modules[0].size = n2_size;
    bi->module_count = 1;

    // --- Framebuffer info ---
    fbinfo_t fb = {0};
    if (!find_framebuffer(SystemTable, &fb)) {
        bi->fb.address = (uint64_t)(uintptr_t)fb.base;
        bi->fb.width   = fb.width;
        bi->fb.height  = fb.height;
        bi->fb.pitch   = fb.pitch;
        bi->fb.bpp     = fb.bpp;
        bi->fb.type    = 0;
        bi->fb.reserved= 0;
    }

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

    // --- ACPI, SMBIOS, etc can be added here if desired ---

    // --- Print status ---
    print_ascii(SystemTable, "[nboot] Bootinfo at ");
    print_hex(SystemTable, (uint64_t)(uintptr_t)bi); print_ascii(SystemTable, "\r\n");
    print_ascii(SystemTable, "[nboot] Jumping to O2.bin...\r\n");

    // --- Call O2.bin's entry (SysV ABI) ---
    typedef void (__attribute__((sysv_abi)) *o2_entry_t)(bootinfo_t *);
    o2_entry_t o2_entry = (o2_entry_t)o2_buf;
    o2_entry(bi);

    return EFI_SUCCESS;
}
