// include/efi.h
#ifndef EFI_H
#define EFI_H

typedef unsigned long long UINTN;
typedef unsigned short CHAR16;
typedef unsigned long long EFI_STATUS;
typedef void* EFI_HANDLE;
typedef void VOID;
typedef unsigned long long EFI_PHYSICAL_ADDRESS;

#define EFI_SUCCESS 0
#define EFI_LOAD_ERROR (EFI_STATUS)(1 | (1ULL << 63))

typedef struct {
    char _pad1[24];
    VOID *ConOut;
    VOID *BootServices;
} EFI_SYSTEM_TABLE;

typedef struct {
    char _pad2[8];
    EFI_STATUS (*OutputString)(VOID *, CHAR16 *);
} EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL;

typedef struct {
    char _pad3[24];
    EFI_STATUS (*ExitBootServices)(
        EFI_HANDLE ImageHandle,
        UINTN MapKey
    );
    // Add more functions as needed
} EFI_BOOT_SERVICES;

#endif
