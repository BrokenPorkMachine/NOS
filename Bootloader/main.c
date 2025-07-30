#include <efi.h>
#include <efilib.h>

#define KERNEL_PATH L"\\EFI\\BOOT\\kernel.bin"
#define KERNEL_MAX_SIZE (128 * 1024 * 1024)
#define KERNEL_LOAD_ADDR 0x100000

EFI_STATUS
efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    InitializeLib(ImageHandle, SystemTable); // Moved to very top for Print output
    Print(L"[Bootloader] Starting UEFI bootloader...\n");

    EFI_STATUS status;
    EFI_FILE_IO_INTERFACE *FileSystem = NULL;
    EFI_FILE_HANDLE RootFS = NULL, KernelFile = NULL;
    EFI_FILE_INFO *FileInfo = NULL;
    UINTN FileInfoSize = 0;
    EFI_PHYSICAL_ADDRESS KernelAddr = KERNEL_LOAD_ADDR;
    UINTN KernelSize = 0, BytesRead = 0;

    // Locate FileSystem
    status = uefi_call_wrapper(BS->HandleProtocol, 3,
                ImageHandle, &gEfiSimpleFileSystemProtocolGuid, (void**)&FileSystem);
    if (EFI_ERROR(status) || !FileSystem) {
        Print(L"[ERR] No filesystem: %r\n", status);
        goto fail;
    }
    // Open root volume
    status = uefi_call_wrapper(FileSystem->OpenVolume, 2, FileSystem, &RootFS);
    if (EFI_ERROR(status) || !RootFS) {
        Print(L"[ERR] OpenVolume: %r\n", status);
        goto fail;
    }
    // Open kernel file
    status = uefi_call_wrapper(RootFS->Open, 5, RootFS, &KernelFile, KERNEL_PATH, EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(status) || !KernelFile) {
        Print(L"[ERR] kernel.bin not found: %r\n", status);
        goto fail;
    }
    // Get file size
    FileInfoSize = SIZE_OF_EFI_FILE_INFO + 200;
    FileInfo = (EFI_FILE_INFO *)AllocatePool(FileInfoSize);
    if (!FileInfo) {
        Print(L"[ERR] Out of memory (FileInfo)\n");
        goto fail;
    }
    status = uefi_call_wrapper(KernelFile->GetInfo, 4, KernelFile, &gEfiFileInfoGuid, &FileInfoSize, FileInfo);
    if (EFI_ERROR(status)) {
        Print(L"[ERR] GetInfo: %r\n", status);
        goto fail;
    }
    KernelSize = FileInfo->FileSize;
    if (KernelSize == 0 || KernelSize > KERNEL_MAX_SIZE) {
        Print(L"[ERR] Kernel size invalid: %ld\n", KernelSize);
        goto fail;
    }
    // Allocate pages for kernel
    status = uefi_call_wrapper(BS->AllocatePages, 4, AllocateAddress, EfiLoaderData,
                               (KernelSize + 0xFFF) / 0x1000, &KernelAddr);
    if (EFI_ERROR(status)) {
        Print(L"[ERR] AllocatePages: %r\n", status);
        goto fail;
    }
    uefi_call_wrapper(BS->SetMem, 3, (void *)KernelAddr, KernelSize, 0);

    // Read kernel file into memory
    BytesRead = KernelSize;
    status = uefi_call_wrapper(KernelFile->Read, 3, KernelFile, &BytesRead, (void *)KernelAddr);
    if (EFI_ERROR(status) || BytesRead != KernelSize) {
        Print(L"[ERR] File read error: %r\n", status);
        goto fail;
    }

    // Clean up handles and buffers
    uefi_call_wrapper(KernelFile->Close, 1, KernelFile);
    uefi_call_wrapper(RootFS->Close, 1, RootFS);
    if (FileInfo) {
        uefi_call_wrapper(BS->SetMem, 3, FileInfo, FileInfoSize, 0);
        FreePool(FileInfo);
    }

    // Compute and output CRC32 for integrity check
    UINT32 crc32 = 0;
    status = uefi_call_wrapper(BS->CalculateCrc32, 3,
                               (void *)KernelAddr, KernelSize, &crc32);
    if (EFI_ERROR(status)) {
        Print(L"[ERR] CalculateCrc32: %r\n", status);
        goto fail;
    }
    Print(L"[Bootloader] Kernel CRC32: %08x\n", crc32);

    // Prepare memory map
    UINTN mmap_size = 0, mmap_key = 0, desc_size = 0;
    UINT32 desc_ver = 0;
    EFI_MEMORY_DESCRIPTOR *mmap = NULL;
    status = uefi_call_wrapper(BS->GetMemoryMap, 5, &mmap_size, mmap, &mmap_key, &desc_size, &desc_ver);
    if (status != EFI_BUFFER_TOO_SMALL) {
        Print(L"[ERR] Unexpected GetMemoryMap: %r\n", status);
        goto fail;
    }
    mmap_size += desc_size * 4;
    mmap = (EFI_MEMORY_DESCRIPTOR *)AllocatePool(mmap_size);
    if (!mmap) {
        Print(L"[ERR] Out of memory (mmap)\n");
        goto fail;
    }
    status = uefi_call_wrapper(BS->GetMemoryMap, 5, &mmap_size, mmap, &mmap_key, &desc_size, &desc_ver);
    if (EFI_ERROR(status)) {
        Print(L"[ERR] Final GetMemoryMap: %r\n", status);
        goto fail;
    }

    // Exit boot services (handoff to kernel)
    status = uefi_call_wrapper(BS->ExitBootServices, 2, ImageHandle, mmap_key);
    if (EFI_ERROR(status)) {
        Print(L"[ERR] ExitBootServices: %r\n", status);
        goto fail;
    }

    Print(L"[Bootloader] Jumping to kernel at 0x%lx\n", KernelAddr);
    void (*kernel_entry)(void) = ((__attribute__((sysv_abi)) void (*)(void))KernelAddr);
    kernel_entry();

    // Unreachable
    while (1);

fail:
    Print(L"[Bootloader] HALT\n");
    return EFI_LOAD_ERROR;
}
