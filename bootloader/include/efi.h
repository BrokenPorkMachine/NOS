#ifndef EFI_H
#define EFI_H

typedef unsigned long long UINTN;
typedef unsigned short CHAR16;
typedef unsigned long long EFI_STATUS;
typedef void* EFI_HANDLE;
typedef void VOID;
typedef unsigned long long EFI_PHYSICAL_ADDRESS;
typedef unsigned int UINT32;

// Allocation types
#define EFI_ALLOCATE_ANY_PAGES   0
#define EFI_ALLOCATE_MAX_ADDRESS 1
#define EFI_ALLOCATE_ADDRESS     2

// Memory types
#define EfiLoaderCode   1
#define EfiLoaderData   2

#define EFI_SUCCESS           0
#define EFI_LOAD_ERROR        (EFI_STATUS)(1 | (1ULL << 63))

typedef struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL;
typedef struct EFI_BOOT_SERVICES EFI_BOOT_SERVICES;
typedef struct EFI_MEMORY_DESCRIPTOR EFI_MEMORY_DESCRIPTOR;

struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL {
    VOID *dummy; // unused
    EFI_STATUS (*OutputString)(
        EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This,
        CHAR16 *String
    );
    // You can add more if needed
};

struct EFI_BOOT_SERVICES {
    char _pad1[40];  // Skip EFI table header and TPL services

    EFI_STATUS (*AllocatePages)(
        UINT32 Type,
        UINT32 MemoryType,
        UINTN Pages,
        EFI_PHYSICAL_ADDRESS *Memory
    );
    EFI_STATUS (*FreePages)(
        EFI_PHYSICAL_ADDRESS Memory,
        UINTN Pages
    );
    EFI_STATUS (*GetMemoryMap)(
        UINTN *MemoryMapSize,
        EFI_MEMORY_DESCRIPTOR *MemoryMap,
        UINTN *MapKey,
        UINTN *DescriptorSize,
        UINT32 *DescriptorVersion
    );

    char _pad2[48];  // Skip to ExitBootServices

    EFI_STATUS (*ExitBootServices)(
        EFI_HANDLE ImageHandle,
        UINTN MapKey
    );
};

struct EFI_MEMORY_DESCRIPTOR {
    UINT32 Type;
    EFI_PHYSICAL_ADDRESS PhysicalStart;
    EFI_PHYSICAL_ADDRESS VirtualStart;
    UINTN NumberOfPages;
    UINT64 Attribute;
};

typedef struct {
    char _pad[44];  // Skip initial EFI table header
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *ConOut;
    EFI_BOOT_SERVICES *BootServices;
} EFI_SYSTEM_TABLE;

#endif
