#include "../include/efi.h"

// --- Basic memory utilities for UEFI freestanding loader ---
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

// --- Minimal print helper (for debugging, not needed in all cases) ---
EFI_STATUS efi_print(EFI_SYSTEM_TABLE *SystemTable, CHAR16 *msg) {
    if (!SystemTable || !SystemTable->ConOut || !msg)
        return EFI_LOAD_ERROR;
    return SystemTable->ConOut->OutputString(SystemTable->ConOut, msg);
}

// --- Example: Allocate pages, open file, read file, etc. (Optional, your loader can do direct protocol calls) ---

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

// Open a file by name from the root directory
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

EFI_STATUS efi_read_file(
    EFI_FILE_PROTOCOL *File,
    UINTN *BufferSize,
    VOID *Buffer
) {
    if (!File || !BufferSize || !Buffer) return EFI_LOAD_ERROR;
    return File->Read(File, BufferSize, Buffer);
}

EFI_STATUS efi_close_file(EFI_FILE_PROTOCOL *File) {
    if (!File) return EFI_INVALID_PARAMETER;
    return File->Close(File);
}
