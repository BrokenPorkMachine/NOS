#include "efi.h"

// Print a Unicode string to UEFI console
EFI_STATUS efi_print(EFI_SYSTEM_TABLE *SystemTable, CHAR16 *msg) {
    if (!SystemTable || !SystemTable->ConOut || !msg) {
        return EFI_LOAD_ERROR;
    }

    return SystemTable->ConOut->OutputString(SystemTable->ConOut, msg);
}

// Allocate pages using EFI_BOOT_SERVICES
EFI_STATUS efi_allocate_pages(EFI_SYSTEM_TABLE *SystemTable, UINT32 allocation_type, UINT32 memory_type, UINTN pages, EFI_PHYSICAL_ADDRESS *memory) {
    if (!SystemTable || !SystemTable->BootServices || !memory) {
        return EFI_LOAD_ERROR;
    }

    return SystemTable->BootServices->AllocatePages(
        allocation_type,
        memory_type,
        pages,
        memory
    );
}

// Get UEFI memory map
EFI_STATUS efi_get_memory_map(EFI_SYSTEM_TABLE *SystemTable, UINTN *memory_map_size, EFI_MEMORY_DESCRIPTOR *memory_map, UINTN *map_key, UINTN *descriptor_size, UINT32 *descriptor_version) {
    if (!SystemTable || !SystemTable->BootServices) {
        return EFI_LOAD_ERROR;
    }

    return SystemTable->BootServices->GetMemoryMap(
        memory_map_size,
        memory_map,
        map_key,
        descriptor_size,
        descriptor_version
    );
}

// Exit UEFI Boot Services
EFI_STATUS efi_exit_boot_services(EFI_SYSTEM_TABLE *SystemTable, EFI_HANDLE ImageHandle, UINTN map_key) {
    if (!SystemTable || !SystemTable->BootServices) {
        return EFI_LOAD_ERROR;
    }

    return SystemTable->BootServices->ExitBootServices(
        ImageHandle,
        map_key
    );
}
