#include "efi.h"
#include "bootinfo.h"
#include "../../include/nosm.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Prototypes for utility functions defined later or elsewhere
static void print_ascii(EFI_SYSTEM_TABLE *st, const char *s);
static EFI_STATUS load_file(EFI_SYSTEM_TABLE *st, EFI_FILE_PROTOCOL *root,
                            const CHAR16 *path, void **buf, UINTN *size);

static void *memcpy(void *dst, const void *src, size_t n) {
    uint8_t *d = dst; const uint8_t *s = src; while (n--) *d++ = *s++; return dst;
}
static void *memset(void *dst, int c, size_t n) {
    uint8_t *d = dst; while (n--) *d++ = (uint8_t)c; return dst;
}
static int memcmp(const void *a, const void *b, size_t n) {
    const uint8_t *x=a, *y=b; while (n--) { if (*x!=*y) return *x-*y; x++; y++; } return 0;
}

// Minimal ELF64 types
typedef struct {
    unsigned char e_ident[16];
    uint16_t e_type, e_machine;
    uint32_t e_version;
    uint64_t e_entry;
    uint64_t e_phoff;
    uint64_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize, e_phentsize, e_phnum, e_shentsize, e_shnum, e_shstrndx;
} Elf64_Ehdr;

typedef struct {
    uint32_t p_type;
    uint32_t p_flags;
    uint64_t p_offset;
    uint64_t p_vaddr;
    uint64_t p_paddr;
    uint64_t p_filesz;
    uint64_t p_memsz;
    uint64_t p_align;
} Elf64_Phdr;

// SHA256 and utility functions are unchanged from your version...
// (Paste your sha256_ctx, sha256_init, sha256_update, sha256_final, compute_sha256, print, print_ascii, etc here.)
// -- for brevity, omitted in this snippet --

// ...[insert your SHA256, print, file, and manifest utilities here]...

// --- Begin main UEFI entry ---
EFI_STATUS EFIAPI efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    print_ascii(SystemTable, "[O2] Booting NitrOS...\r\n");

    // Secure Boot check
    UINT8 secure = 0; UINTN ssz = sizeof(secure);
    bool secure_boot = false;
    EFI_STATUS status = SystemTable->RuntimeServices->GetVariable(
        L"SecureBoot", (EFI_GUID*)&gEfiGlobalVariableGuid, NULL, &ssz, &secure);
    if (EFI_ERROR(status) || secure == 0) {
        print_ascii(SystemTable, "[O2] Secure Boot disabled\r\n");
    } else {
        print_ascii(SystemTable, "[O2] Secure Boot enabled\r\n");
        (void)secure_boot = true;
    }

    EFI_LOADED_IMAGE_PROTOCOL *loaded;
    status = SystemTable->BootServices->HandleProtocol(ImageHandle,
        (EFI_GUID*)&gEfiLoadedImageProtocolGuid, (void**)&loaded);
    if (EFI_ERROR(status)) return status;

    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs;
    status = SystemTable->BootServices->HandleProtocol(loaded->DeviceHandle,
        (EFI_GUID*)&gEfiSimpleFileSystemProtocolGuid, (void**)&fs);
    if (EFI_ERROR(status)) return status;

    EFI_FILE_PROTOCOL *root;
    status = fs->OpenVolume(fs, &root);
    if (EFI_ERROR(status)) return status;

    bootinfo_t *bi;
    status = SystemTable->BootServices->AllocatePool(EfiLoaderData, sizeof(bootinfo_t), (void**)&bi);
    if (EFI_ERROR(status)) return status;
    memset(bi, 0, sizeof(*bi));
    bi->magic = BOOTINFO_MAGIC_UEFI;
    bi->size = sizeof(*bi);
    bi->bootloader_name = "O2 UEFI";

    // --- Load kernel ELF ---
    void *kernel_file; UINTN kernel_size;
    status = load_file(SystemTable, root, L"\\kernel.bin", &kernel_file, &kernel_size);
    if (EFI_ERROR(status)) return status;

    Elf64_Ehdr *eh = (Elf64_Ehdr*)kernel_file;
    if (eh->e_ident[0] != 0x7F || eh->e_ident[1] != 'E' ||
        eh->e_ident[2] != 'L'  || eh->e_ident[3] != 'F' ||
        eh->e_ident[4] != 2 /* ELFCLASS64 */) {
        print_ascii(SystemTable, "[O2] Kernel is not ELF64\n");
        return EFI_LOAD_ERROR;
    }

    // Optional: kernel signature check here (use compute_sha256 etc)

    // Load ELF PT_LOAD segments
    for (int i = 0; i < eh->e_phnum; ++i) {
        Elf64_Phdr *ph = (Elf64_Phdr *)((uint8_t *)kernel_file + eh->e_phoff + i * eh->e_phentsize);
        if (ph->p_type != 1 /* PT_LOAD */) continue;

        UINTN seg_size = ph->p_memsz;
        UINTN seg_offset = ph->p_offset;
        UINTN seg_vaddr = ph->p_vaddr;
        void *seg_dest = (void *)(uintptr_t)seg_vaddr;

        // Allocate pages for segment
        EFI_PHYSICAL_ADDRESS seg_phys = seg_vaddr;
        status = SystemTable->BootServices->AllocatePages(
            AllocateAddress, EfiLoaderData, (seg_size + 0xFFF) / 0x1000, &seg_phys);
        if (EFI_ERROR(status)) {
            print_ascii(SystemTable, "[O2] Failed to allocate kernel segment\n");
            return status;
        }
        // Copy segment data
        memcpy(seg_dest, (uint8_t*)kernel_file + seg_offset, ph->p_filesz);
        // Zero out any .bss
        if (ph->p_memsz > ph->p_filesz) {
            memset((uint8_t*)seg_dest + ph->p_filesz, 0, ph->p_memsz - ph->p_filesz);
        }
    }
    // Optionally free file buffer
    SystemTable->BootServices->FreePool(kernel_file);

    // Set up kernel entry for bootinfo
    bi->kernel_entry = (void *)(uintptr_t)eh->e_entry;
    bi->kernel_segs.file_base = (uint64_t)(uintptr_t)eh->e_entry; // Or vaddr of .text
    bi->kernel_segs.file_size = kernel_size;

    // --- Load modules, framebuffer, ACPI, memory map ---
    // [Paste your module loader, framebuffer, ACPI, memory map, as in your old code here.]

    // Example: continue with your modules and device setup as before.
    // --- Load modules, set bi->modules, bi->module_count, etc. ---
    // --- Set up graphics/framebuffer, ACPI, etc. ---

    // Memory map as before
    UINTN mmapSize = 0, mapKey = 0, descSize = 0; UINT32 descVer = 0;
    SystemTable->BootServices->GetMemoryMap(&mmapSize, NULL, &mapKey, &descSize, &descVer);
    mmapSize += descSize * 2;
    status = SystemTable->BootServices->AllocatePool(EfiLoaderData, mmapSize, (void**)&bi->mmap);
    if (EFI_ERROR(status)) return status;
    status = SystemTable->BootServices->GetMemoryMap(&mmapSize, bi->mmap, &mapKey, &descSize, &descVer);
    if (EFI_ERROR(status)) return status;
    bi->mmap_entries = mmapSize / descSize;
    bi->reserved[0] = bi->mmap_entries;

    // --- Exit Boot Services ---
    status = SystemTable->BootServices->ExitBootServices(ImageHandle, mapKey);
    if (EFI_ERROR(status)) return status;

    // --- Jump to kernel entry ---
    void (*kernel_entry)(bootinfo_t*) = (void(*)(bootinfo_t*))bi->kernel_entry;
    kernel_entry(bi);
    return EFI_SUCCESS;
}
