#include "../include/efi.h"
#include "../include/bootinfo.h"

#define KERNEL_PATH L"\\EFI\\BOOT\\kernel.bin"
#define KERNEL_MAX_SIZE (2 * 1024 * 1024)
#define MAX_MMAP_ENTRIES 128

EFI_STATUS efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    EFI_STATUS status;
    EFI_BOOT_SERVICES *BS = SystemTable->BootServices;
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *ConOut = SystemTable->ConOut;

    // --- 1. Prepare bootinfo struct ---
    bootinfo_t *info = NULL;
    BS->AllocatePages(EFI_ALLOCATE_ANY_PAGES, EfiLoaderData, 1, (EFI_PHYSICAL_ADDRESS*)&info);
    SetMem(info, sizeof(bootinfo_t), 0);
    info->magic = BOOTINFO_MAGIC_UEFI;
    info->size = sizeof(bootinfo_t);

    // --- 2. Memory map ---
    UINTN mmap_size = 0, map_key, desc_size;
    UINT32 desc_ver;
    BS->GetMemoryMap(&mmap_size, NULL, &map_key, &desc_size, &desc_ver);
    mmap_size += desc_size * 16;
    EFI_MEMORY_DESCRIPTOR *efi_mmap;
    BS->AllocatePages(EFI_ALLOCATE_ANY_PAGES, EfiLoaderData, (mmap_size + 4095) / 4096, (EFI_PHYSICAL_ADDRESS*)&efi_mmap);
    status = BS->GetMemoryMap(&mmap_size, efi_mmap, &map_key, &desc_size, &desc_ver);

    // Fill bootinfo memory map:
    bootinfo_memory_t *mmap = NULL;
    BS->AllocatePages(EFI_ALLOCATE_ANY_PAGES, EfiLoaderData, 1, (EFI_PHYSICAL_ADDRESS*)&mmap);
    int mmap_count = 0;
    for (UINT8 *p = (UINT8*)efi_mmap; p < (UINT8*)efi_mmap + mmap_size; p += desc_size) {
        EFI_MEMORY_DESCRIPTOR *d = (EFI_MEMORY_DESCRIPTOR*)p;
        mmap[mmap_count].addr = d->PhysicalStart;
        mmap[mmap_count].len = d->NumberOfPages * 4096;
        mmap[mmap_count].type = d->Type;
        mmap[mmap_count].reserved = 0;
        mmap_count++;
        if (mmap_count >= MAX_MMAP_ENTRIES) break;
    }
    info->mmap = mmap;
    info->mmap_entries = mmap_count;

    // --- 3. Framebuffer ---
    EFI_GUID gop_guid = {0x9042a9de,0x23dc,0x4a38,{0x96,0xfb,0x7a,0xde,0xd0,0x80,0x51,0x6a}};
    struct {
        UINTN Size;
        VOID *Buffer;
    } gop_handle = {0};
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
    } else {
        info->framebuffer = 0;
    }

    // --- 4. ACPI RSDP detection ---
    EFI_GUID acpi2_guid = {0x8868e871,0xe4f1,0x11d3,{0xbc,0x22,0x00,0x80,0xc7,0x3c,0x88,0x81}};
    VOID *rsdp = 0;
    status = BS->LocateProtocol(&acpi2_guid, NULL, &rsdp);
    info->acpi_rsdp = (uint64_t)rsdp;

    // --- 5. SMP (Not provided by UEFI -- see below for Multiboot) ---
    info->smp = 0;

    // --- 6. Modules, cmdline, loader name (populate as needed) ---
    info->module_count = 0;
    info->cmdline = NULL;
    info->bootloader_name = L"NitrOBoot UEFI";

    // --- 7. Read kernel, parse ELF, load segments as before (not repeated here) ---

    // --- 8. ExitBootServices ---
    status = BS->ExitBootServices(ImageHandle, map_key);

    // --- 9. Jump to kernel ---
    void (*entry)(bootinfo_t*) = (void (*)(bootinfo_t*))(your_kernel_entry);
    entry(info);
    while (1);

    return EFI_SUCCESS;
}
