#ifndef EFI_H
#define EFI_H

typedef unsigned long long UINTN;
typedef unsigned short CHAR16;
typedef unsigned long long EFI_STATUS;
typedef void* EFI_HANDLE;
typedef void VOID;
typedef unsigned long long EFI_PHYSICAL_ADDRESS;
typedef unsigned int UINT32;

#define EFI_SUCCESS           0
#define EFI_LOAD_ERROR        (EFI_STATUS)(1 | (1ULL << 63))

typedef struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL;
typedef struct EFI_BOOT_SERVICES EFI_BOOT_SERVICES;

struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL {
    VOID *dummy; // unused
    EFI_STATUS (*OutputString)(
        EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This,
        CHAR16 *String
    );
    // You can add more if needed
};

struct EFI_BOOT_SERVICES {
    char _pad1[24];  // Skip initial table fields (header and some functions)
    
    // Add AllocatePages at the correct offset (simplified version)
    EFI_STATUS (*AllocatePages)(
        UINT32 Type,
        UINT32 MemoryType,
        UINTN Pages,
        EFI_PHYSICAL_ADDRESS *Memory
    );

    char _pad2[64];  // Placeholder padding, adjust if more functions are used

    EFI_STATUS (*ExitBootServices)(
        EFI_HANDLE ImageHandle,
        UINTN MapKey
    );
};

typedef struct {
    char _pad[44];  // Skip initial EFI table header
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *ConOut;
    EFI_BOOT_SERVICES *BootServices;
} EFI_SYSTEM_TABLE;

#endif
