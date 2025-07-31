// src/NitrOBoot.c
#include "../include/efi.h"

EFI_STATUS efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    SystemTable->ConOut->OutputString(SystemTable->ConOut, L"NitrOBoot Loader Starting...\r\n");

    EFI_BOOT_SERVICES *BS = SystemTable->BootServices;
    
    // (1) Here you open 'kernel.bin' from the FAT filesystem (left minimal here for brevity)
    // (You can extend using EFI_SIMPLE_FILE_SYSTEM_PROTOCOL)

    EFI_PHYSICAL_ADDRESS kernel_base = 0x100000; // 1MB, typical kernel load addr
    UINTN kernel_size = 0x200000; // 2MB max size, adjust as needed
    
    // (2) Allocate pages for kernel
    EFI_STATUS status = BS->AllocatePages(
        EFI_ALLOCATE_ADDRESS,
        EfiLoaderData,
        (kernel_size + 4095) / 4096,
        &kernel_base
    );
    if (status != EFI_SUCCESS) {
        SystemTable->ConOut->OutputString(SystemTable->ConOut, L"Allocation failed\r\n");
        return status;
    }

    // (3) Load your kernel.bin into kernel_base here...
    // (Left as an exercise, but simple File I/O through EFI interfaces)

    SystemTable->ConOut->OutputString(SystemTable->ConOut, L"Kernel loaded\r\n");

    // (4) Get memory map and ExitBootServices
    UINTN map_size = 0;
    UINTN map_key = 0;
    UINTN desc_size = 0;
    UINT32 desc_ver = 0;

    // First call to get required buffer size
    BS->GetMemoryMap(&map_size, (EFI_MEMORY_DESCRIPTOR *)0, &map_key, &desc_size, &desc_ver);
    EFI_PHYSICAL_ADDRESS mmap_buf = 0;
    status = BS->AllocatePages(EFI_ALLOCATE_ANY_PAGES, EfiLoaderData,
                               (map_size + 4095) / 4096, &mmap_buf);
    if (status != EFI_SUCCESS)
        return status;

    status = BS->GetMemoryMap(&map_size, (EFI_MEMORY_DESCRIPTOR *)mmap_buf,
                              &map_key, &desc_size, &desc_ver);
    if (status != EFI_SUCCESS)
        return status;

    status = BS->ExitBootServices(ImageHandle, map_key);
    if (status != EFI_SUCCESS) {
        return status;
    }

    // (5) Transfer execution to kernel (custom handoff)
    void (*kernel_entry)(void*) = (void (*)(void*))kernel_base;
    kernel_entry((void*)0); // pass info struct pointer as needed
    
    for (;;);
    return EFI_SUCCESS;
}
