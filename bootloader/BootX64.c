#include <efi.h>
#include <efilib.h>

EFI_STATUS EFIAPI efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
{
    InitializeLib(ImageHandle, SystemTable);
    Print(L"NitrOS bootloader via gnu-efi!\n");
    /* TODO: load kernel here */
    while (1);
    return EFI_SUCCESS;
}
