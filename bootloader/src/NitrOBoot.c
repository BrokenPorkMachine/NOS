// src/NitrOBoot.c
#include "../include/efi.h"

#define KERNEL_PATH L"\\EFI\\BOOT\\kernel.bin"
#define KERNEL_BASE_ADDR 0x100000
#define KERNEL_MAX_SIZE (2 * 1024 * 1024)

EFI_STATUS efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    EFI_STATUS status;
    EFI_BOOT_SERVICES *BS = SystemTable->BootServices;
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *ConOut = SystemTable->ConOut;

    ConOut->OutputString(ConOut, L"NitrOBoot Loader Starting...\r\n");

    // (1) Locate Simple FileSystem Protocol
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *FileSystem;
    status = BS->HandleProtocol(ImageHandle, &gEfiSimpleFileSystemProtocolGuid, (VOID**)&FileSystem);
    if (status != EFI_SUCCESS) {
        ConOut->OutputString(ConOut, L"Error: Can't locate FileSystem protocol\r\n");
        return status;
    }

    EFI_FILE_PROTOCOL *Root;
    status = FileSystem->OpenVolume(FileSystem, &Root);
    if (status != EFI_SUCCESS) {
        ConOut->OutputString(ConOut, L"Error: Can't open root directory\r\n");
        return status;
    }

    EFI_FILE_PROTOCOL *KernelFile;
    status = Root->Open(Root, &KernelFile, KERNEL_PATH, EFI_FILE_MODE_READ, 0);
    if (status != EFI_SUCCESS) {
        ConOut->OutputString(ConOut, L"Error: Can't open kernel.bin\r\n");
        return status;
    }

    // (2) Allocate pages for kernel
    EFI_PHYSICAL_ADDRESS kernel_base = KERNEL_BASE_ADDR;
    UINTN kernel_pages = (KERNEL_MAX_SIZE + 4095) / 4096;
    status = BS->AllocatePages(EFI_ALLOCATE_ADDRESS, EfiLoaderData, kernel_pages, &kernel_base);
    if (status != EFI_SUCCESS) {
        ConOut->OutputString(ConOut, L"Error: Allocation failed\r\n");
        return status;
    }

    // (3) Read kernel file into memory
    UINTN kernel_size = KERNEL_MAX_SIZE;
    status = KernelFile->Read(KernelFile, &kernel_size, (VOID*)kernel_base);
    KernelFile->Close(KernelFile);

    if (status != EFI_SUCCESS || kernel_size == 0) {
        ConOut->OutputString(ConOut, L"Error: Failed to read kernel.bin\r\n");
        return EFI_LOAD_ERROR;
    }

    ConOut->OutputString(ConOut, L"Kernel loaded successfully\r\n");

    // (4) Get memory map for ExitBootServices
    UINTN mmap_size = 0, map_key, desc_size;
    UINT32 desc_ver;
    BS->GetMemoryMap(&mmap_size, NULL, &map_key, &desc_size, &desc_ver);
    mmap_size += desc_size * 8;

    EFI_PHYSICAL_ADDRESS mmap_addr;
    status = BS->AllocatePages(EFI_ALLOCATE_ANY_PAGES, EfiLoaderData, (mmap_size + 4095) / 4096, &mmap_addr);
    if (status != EFI_SUCCESS) {
        ConOut->OutputString(ConOut, L"Error: Can't allocate memory for memory map\r\n");
        return status;
    }

    EFI_MEMORY_DESCRIPTOR *memory_map = (EFI_MEMORY_DESCRIPTOR*)(UINTN)mmap_addr;
    status = BS->GetMemoryMap(&mmap_size, memory_map, &map_key, &desc_size, &desc_ver);
    if (status != EFI_SUCCESS) {
        ConOut->OutputString(ConOut, L"Error: GetMemoryMap failed\r\n");
        return status;
    }

    // (5) Exit Boot Services
    status = BS->ExitBootServices(ImageHandle, map_key);
    if (status != EFI_SUCCESS) {
        ConOut->OutputString(ConOut, L"Error: ExitBootServices failed\r\n");
        return status;
    }

    // (6) Jump to kernel entry point
    void (*kernel_entry)(void *) = (void (*)(void *))kernel_base;
    kernel_entry(NULL); // Pass a structure here if needed

    // (7) Should never return here
    for (;;);

    return EFI_SUCCESS;
}
