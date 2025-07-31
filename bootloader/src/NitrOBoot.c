// src/NitrOBoot.c
#include "../include/efi.h"

#define KERNEL_PATH L"\\EFI\\BOOT\\kernel.bin"
#define KERNEL_BASE_ADDR 0x100000
#define KERNEL_MAX_SIZE (2 * 1024 * 1024)

static void print_step(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *ConOut, CHAR16 *msg) {
    ConOut->SetAttribute(ConOut, EFI_TEXT_ATTR(EFI_YELLOW, EFI_BLACK));
    ConOut->OutputString(ConOut, msg);
    ConOut->SetAttribute(ConOut, EFI_TEXT_ATTR(EFI_LIGHTGRAY, EFI_BLACK));
}

static void print_ok(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *ConOut) {
    ConOut->SetAttribute(ConOut, EFI_TEXT_ATTR(EFI_LIGHTGREEN, EFI_BLACK));
    ConOut->OutputString(ConOut, L" [OK]\r\n");
    ConOut->SetAttribute(ConOut, EFI_TEXT_ATTR(EFI_LIGHTGRAY, EFI_BLACK));
}

static void print_fail(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *ConOut, CHAR16 *msg) {
    ConOut->SetAttribute(ConOut, EFI_TEXT_ATTR(EFI_LIGHTRED, EFI_BLACK));
    ConOut->OutputString(ConOut, L" [FAIL]\r\n");
    if (msg) ConOut->OutputString(ConOut, msg);
    ConOut->SetAttribute(ConOut, EFI_TEXT_ATTR(EFI_LIGHTGRAY, EFI_BLACK));
}

EFI_STATUS efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    EFI_STATUS status;
    EFI_BOOT_SERVICES *BS = SystemTable->BootServices;
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *ConOut = SystemTable->ConOut;

    ConOut->SetAttribute(ConOut, EFI_TEXT_ATTR(EFI_WHITE, EFI_BLUE));
    ConOut->ClearScreen(ConOut);
    ConOut->OutputString(ConOut, L"NitrOBoot Loader Starting...\r\n");
    ConOut->SetAttribute(ConOut, EFI_TEXT_ATTR(EFI_LIGHTGRAY, EFI_BLACK));

    // (1) Locate Simple FileSystem Protocol
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *FileSystem;
    print_step(ConOut, L"[1] Locate FileSystem protocol...");
    status = BS->HandleProtocol(ImageHandle, &gEfiSimpleFileSystemProtocolGuid, (VOID**)&FileSystem);
    if (status != EFI_SUCCESS) {
        print_fail(ConOut, L"Error: Can't locate FileSystem protocol\r\n");
        return status;
    }
    print_ok(ConOut);

    EFI_FILE_PROTOCOL *Root;
    print_step(ConOut, L"[2] Open root volume...");
    status = FileSystem->OpenVolume(FileSystem, &Root);
    if (status != EFI_SUCCESS) {
        print_fail(ConOut, L"Error: Can't open root directory\r\n");
        return status;
    }
    print_ok(ConOut);

    EFI_FILE_PROTOCOL *KernelFile;
    print_step(ConOut, L"[3] Open kernel file...");
    status = Root->Open(Root, &KernelFile, KERNEL_PATH, EFI_FILE_MODE_READ, 0);
    if (status != EFI_SUCCESS) {
        print_fail(ConOut, L"Error: Can't open kernel.bin\r\n");
        return status;
    }
    print_ok(ConOut);

    // (2) Allocate pages for kernel
    EFI_PHYSICAL_ADDRESS kernel_base = KERNEL_BASE_ADDR;
    UINTN kernel_pages = (KERNEL_MAX_SIZE + 4095) / 4096;
    print_step(ConOut, L"[4] Allocate pages for kernel...");
    status = BS->AllocatePages(EFI_ALLOCATE_ADDRESS, EfiLoaderData, kernel_pages, &kernel_base);
    if (status != EFI_SUCCESS) {
        print_fail(ConOut, L"Error: Allocation failed\r\n");
        return status;
    }
    print_ok(ConOut);

    // (3) Read kernel file into memory
    UINTN kernel_size = KERNEL_MAX_SIZE;
    print_step(ConOut, L"[5] Read kernel into memory...");
    status = KernelFile->Read(KernelFile, &kernel_size, (VOID*)kernel_base);
    KernelFile->Close(KernelFile);

    if (status != EFI_SUCCESS || kernel_size == 0) {
        print_fail(ConOut, L"Error: Failed to read kernel.bin\r\n");
        return EFI_LOAD_ERROR;
    }
    print_ok(ConOut);

    // (4) Get memory map for ExitBootServices
    UINTN mmap_size = 0, map_key, desc_size;
    UINT32 desc_ver;
    print_step(ConOut, L"[6] Obtain memory map...");
    BS->GetMemoryMap(&mmap_size, NULL, &map_key, &desc_size, &desc_ver);
    mmap_size += desc_size * 8;

    EFI_PHYSICAL_ADDRESS mmap_addr;
    status = BS->AllocatePages(EFI_ALLOCATE_ANY_PAGES, EfiLoaderData, (mmap_size + 4095) / 4096, &mmap_addr);
    if (status != EFI_SUCCESS) {
        print_fail(ConOut, L"Error: Can't allocate memory for memory map\r\n");
        return status;
    }

    EFI_MEMORY_DESCRIPTOR *memory_map = (EFI_MEMORY_DESCRIPTOR*)(UINTN)mmap_addr;
    status = BS->GetMemoryMap(&mmap_size, memory_map, &map_key, &desc_size, &desc_ver);
    if (status != EFI_SUCCESS) {
        print_fail(ConOut, L"Error: GetMemoryMap failed\r\n");
        return status;
    }
    print_ok(ConOut);

    // (5) Exit Boot Services
    print_step(ConOut, L"[7] Exit boot services...");
    status = BS->ExitBootServices(ImageHandle, map_key);
    if (status != EFI_SUCCESS) {
        print_fail(ConOut, L"Error: ExitBootServices failed\r\n");
        return status;
    }
    print_ok(ConOut);

    // (6) Jump to kernel entry point
    print_step(ConOut, L"[8] Jumping to kernel...\r\n");
    void (*kernel_entry)(void *) = (void (*)(void *))kernel_base;
    kernel_entry(NULL); // Pass a structure here if needed

    // (7) Should never return here
    for (;;);

    return EFI_SUCCESS;
}
