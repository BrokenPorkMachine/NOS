// src/efi.c
#include "../include/efi.h"

// Print a Unicode string to the UEFI console (null-safe)
EFI_STATUS efi_print(EFI_SYSTEM_TABLE *SystemTable, CHAR16 *msg) {
    if (!SystemTable || !SystemTable->ConOut || !msg)
        return EFI_LOAD_ERROR;
    return SystemTable->ConOut->OutputString(SystemTable->ConOut, msg);
}

// Allocate pages using EFI_BOOT_SERVICES
EFI_STATUS efi_allocate_pages(
    EFI_SYSTEM_TABLE *SystemTable,
    UINTN allocation_type,
    UINTN memory_type,
    UINTN pages,
    EFI_PHYSICAL_ADDRESS *memory
) {
    if (!SystemTable || !SystemTable->BootServices || !memory)
        return EFI_LOAD_ERROR;
    return SystemTable->BootServices->AllocatePages(
        allocation_type,
        memory_type,
        pages,
        memory
    );
}

// Get the UEFI memory map
EFI_STATUS efi_get_memory_map(
    EFI_SYSTEM_TABLE *SystemTable,
    UINTN *memory_map_size,
    EFI_MEMORY_DESCRIPTOR *memory_map,
    UINTN *map_key,
    UINTN *descriptor_size,
    UINT32 *descriptor_version
) {
    if (!SystemTable || !SystemTable->BootServices)
        return EFI_LOAD_ERROR;
    return SystemTable->BootServices->GetMemoryMap(
        memory_map_size,
        memory_map,
        map_key,
        descriptor_size,
        descriptor_version
    );
}

// Exit UEFI Boot Services
EFI_STATUS efi_exit_boot_services(
    EFI_SYSTEM_TABLE *SystemTable,
    EFI_HANDLE ImageHandle,
    UINTN map_key
) {
    if (!SystemTable || !SystemTable->BootServices)
        return EFI_LOAD_ERROR;
    return SystemTable->BootServices->ExitBootServices(
        ImageHandle,
        map_key
    );
}

// Open the root volume of the filesystem on which the EFI application is located
EFI_STATUS efi_open_root_volume(
    EFI_SYSTEM_TABLE *SystemTable,
    EFI_HANDLE ImageHandle,
    EFI_FILE_PROTOCOL **Root
) {
    if (!SystemTable || !Root) return EFI_LOAD_ERROR;
    EFI_STATUS status;
    EFI_BOOT_SERVICES *BS = SystemTable->BootServices;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs;
    status = BS->HandleProtocol(
        ImageHandle,
        (EFI_GUID *)&gEfiSimpleFileSystemProtocolGuid,
        (void **)&fs
    );
    if (status != EFI_SUCCESS) return status;
    return fs->OpenVolume(fs, Root);
}

// Open a file from EFI filesystem
EFI_STATUS efi_open_file(
    EFI_FILE_PROTOCOL *Root,
    CHAR16 *FileName,
    EFI_FILE_PROTOCOL **File
) {
    if (!Root || !FileName || !File) return EFI_LOAD_ERROR;
    return Root->Open(
        Root,
        File,
        FileName,
        EFI_FILE_MODE_READ,
        0
    );
}

// Read data from an opened EFI file
EFI_STATUS efi_read_file(
    EFI_FILE_PROTOCOL *File,
    UINTN *BufferSize,
    VOID *Buffer
) {
    if (!File || !BufferSize || !Buffer) return EFI_LOAD_ERROR;
    return File->Read(File, BufferSize, Buffer);
}

// Close an opened EFI file
EFI_STATUS efi_close_file(EFI_FILE_PROTOCOL *File) {
    if (!File) return EFI_INVALID_PARAMETER;
    return File->Close(File);
}

// --- Basic memory utilities ---
// These are usually available in most toolchains, but for freestanding code we define them here.

VOID *EFIAPI CopyMem(VOID *Destination, const VOID *Source, UINTN Length) {
    UINT8 *d = (UINT8 *)Destination;
    const UINT8 *s = (const UINT8 *)Source;
    for (UINTN i = 0; i < Length; ++i) d[i] = s[i];
    return Destination;
}

VOID *EFIAPI SetMem(VOID *Buffer, UINTN Size, UINT8 Value) {
    UINT8 *d = (UINT8 *)Buffer;
    for (UINTN i = 0; i < Size; ++i) d[i] = Value;
    return Buffer;
}
