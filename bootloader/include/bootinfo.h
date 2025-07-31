#include "../include/efi.h"
#include "../include/bootinfo.h"

#define KERNEL_PATH L"\\EFI\\BOOT\\kernel.bin"
#define KERNEL_MAX_SIZE (2 * 1024 * 1024)
#define MAX_MMAP_ENTRIES 128

// Minimal prototype for kernel entry (fill this with your real logic)
extern void kernel_entry(bootinfo_t *info);

// Helper: Print UEFI string
static void efi_print(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *ConOut, const CHAR16 *msg) {
    ConOut->OutputString(ConOut, (CHAR16*)msg);
}

// Helper: Print unsigned long in hex (to UEFI screen)
static void efi_print_hex(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *ConOut, UINT64 v) {
    CHAR16 buf[20] = L"0x0000000000000000\r\n";
    for (int i = 0; i < 16; ++i)
        buf[17 - i] = L"0123456789ABCDEF"[(v >> (i * 4)) & 0xF];
    ConOut->OutputString(ConOut, buf);
}

// Helper: Fill framebuffer with a color
static void fill_framebuffer(bootinfo_framebuffer_t *fb, UINT32 color) {
    if (!fb || !fb->address) return;
    UINT32 *ptr = (UINT32*)(UINTN)fb->address;
    UINT32 pixels = fb->height * (fb->pitch / 4);
    for (UINT32 i = 0; i < pixels; ++i)
        ptr[i] = color;
}

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
    UINTN mmap_size = 0, map_key = 0, desc_size = 0;
    UINT32 desc_ver = 0;
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

    // Print all RAM regions
    efi_print(ConOut, L"Physical Memory Map:\r\n");
    for (int i = 0; i < mmap_count; ++i) {
        efi_print(ConOut, L"  [");
        efi_print_hex(ConOut, mmap[i].addr);
        efi_print(ConOut, L"-");
        efi_print_hex(ConOut, mmap[i].addr + mmap[i].len);
        efi_print(ConOut, L"] type=");
        efi_print_hex(ConOut, mmap[i].type);
        efi_print(ConOut, L"\r\n");
    }

    // --- 3. Framebuffer ---
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

        // Fill framebuffer with blue to indicate loader success
        fill_framebuffer(fb, 0xFF0000FF); // BGRA = blue
        efi_print(ConOut, L"Framebuffer present, screen filled with blue.\r\n");
    } else {
        info->framebuffer = 0;
        efi_print(ConOut, L"No framebuffer provided.\r\n");
    }

    // --- 4. ACPI RSDP detection ---
    EFI_GUID acpi2_guid = {0x8868e871,0xe4f1,0x11d3,{0xbc,0x22,0x00,0x80,0xc7,0x3c,0x88,0x81}};
    VOID *rsdp = 0;
    status = BS->LocateProtocol(&acpi2_guid, NULL, &rsdp);
    info->acpi_rsdp = (uint64_t)rsdp;
    if (rsdp) {
        efi_print(ConOut, L"ACPI RSDP detected at: ");
        efi_print_hex(ConOut, (UINT64)rsdp);
        efi_print(ConOut, L"\r\n");
    } else {
        efi_print(ConOut, L"No ACPI RSDP found.\r\n");
    }

    // --- 5. SMP (Not provided by UEFI) ---
    info->smp = 0;

    // --- 6. Modules, cmdline, loader name (populate as needed) ---
    info->module_count = 0;
    info->cmdline = NULL;
    static CHAR16 boot_name[] = L"NitrOBoot UEFI";
    info->bootloader_name = boot_name;

    // --- 7. ELF Load kernel.bin here (stub, replace with real ELF loader) ---
    // ... (insert your ELF load code, use info->framebuffer etc. as needed) ...

    // --- 8. ExitBootServices with retry for dynamic memory map changes ---
    UINTN try_map_key;
    for (int tries = 0; tries < 16; ++tries) {
        UINTN temp_map_size = mmap_size;
        status = BS->GetMemoryMap(&temp_map_size, efi_mmap, &try_map_key, &desc_size, &desc_ver);
        status = BS->ExitBootServices(ImageHandle, try_map_key);
        if (status == EFI_SUCCESS) break;
        if (tries == 15) {
            efi_print(ConOut, L"ExitBootServices failed after multiple tries.\r\n");
            return status;
        }
    }

    // --- 9. Jump to kernel ---
    kernel_entry(info);
    while (1);

    return EFI_SUCCESS;
}
