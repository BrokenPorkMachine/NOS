#ifndef EFI_H
#define EFI_H

// Standard UEFI Types
typedef unsigned char       UINT8;
typedef unsigned short      UINT16;
typedef unsigned int        UINT32;
typedef unsigned long long  UINT64;
typedef unsigned long long  UINTN;
typedef unsigned short      CHAR16;
typedef unsigned char       BOOLEAN;
typedef void                VOID;

typedef UINTN               EFI_STATUS;
typedef VOID*               EFI_HANDLE;
typedef UINT64              EFI_PHYSICAL_ADDRESS;
typedef UINT64              EFI_VIRTUAL_ADDRESS;

#ifndef NULL
#define NULL ((void *)0)
#endif

typedef struct {
    UINT32  Data1;
    UINT16  Data2;
    UINT16  Data3;
    UINT8   Data4[8];
} EFI_GUID;

// UEFI Status Codes
#define EFI_SUCCESS               0
#define EFI_LOAD_ERROR            (EFI_STATUS)(1ULL | (1ULL << 63))
#define EFI_OUT_OF_RESOURCES      (EFI_STATUS)(9ULL | (1ULL << 63))
#define EFI_INVALID_PARAMETER     (EFI_STATUS)(2ULL | (1ULL << 63))
#define EFI_BUFFER_TOO_SMALL      (EFI_STATUS)(5ULL | (1ULL << 63))
#define EFI_NOT_FOUND             (EFI_STATUS)(14ULL | (1ULL << 63))

// Text output colors and attributes
#define EFI_BLACK        0x0
#define EFI_BLUE         0x1
#define EFI_GREEN        0x2
#define EFI_CYAN         0x3
#define EFI_RED          0x4
#define EFI_MAGENTA      0x5
#define EFI_BROWN        0x6
#define EFI_LIGHTGRAY    0x7
#define EFI_DARKGRAY     0x8
#define EFI_LIGHTBLUE    0x9
#define EFI_LIGHTGREEN   0xA
#define EFI_LIGHTCYAN    0xB
#define EFI_LIGHTRED     0xC
#define EFI_LIGHTMAGENTA 0xD
#define EFI_YELLOW       0xE
#define EFI_WHITE        0xF

#define EFI_TEXT_ATTR(fg, bg) ((fg) | ((bg) << 4))

// Page Allocation Types
#define EFI_ALLOCATE_ANY_PAGES      0
#define EFI_ALLOCATE_MAX_ADDRESS    1
#define EFI_ALLOCATE_ADDRESS        2

// Memory Types
#define EfiReservedMemoryType       0
#define EfiLoaderCode               1
#define EfiLoaderData               2
#define EfiBootServicesCode         3
#define EfiBootServicesData         4
#define EfiRuntimeServicesCode      5
#define EfiRuntimeServicesData      6
#define EfiConventionalMemory       7
#define EfiUnusableMemory           8
#define EfiACPIReclaimMemory        9
#define EfiACPIMemoryNVS            10
#define EfiMemoryMappedIO           11
#define EfiMemoryMappedIOPortSpace  12
#define EfiPalCode                  13

// Forward Declarations
typedef struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL;
typedef struct EFI_BOOT_SERVICES EFI_BOOT_SERVICES;
typedef struct EFI_MEMORY_DESCRIPTOR EFI_MEMORY_DESCRIPTOR;
typedef struct EFI_FILE_PROTOCOL EFI_FILE_PROTOCOL;
typedef struct EFI_SIMPLE_FILE_SYSTEM_PROTOCOL EFI_SIMPLE_FILE_SYSTEM_PROTOCOL;

// EFI Simple Text Output Protocol
struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL {
    EFI_STATUS (*Reset)(
        EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This,
        BOOLEAN ExtendedVerification
    );
    EFI_STATUS (*OutputString)(
        EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This,
        CHAR16 *String
    );
    EFI_STATUS (*TestString)(
        EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This,
        CHAR16 *String
    );
    EFI_STATUS (*QueryMode)(
        EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This,
        UINTN ModeNumber,
        UINTN *Columns,
        UINTN *Rows
    );
    EFI_STATUS (*SetMode)(
        EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This,
        UINTN ModeNumber
    );
    EFI_STATUS (*SetAttribute)(
        EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This,
        UINTN Attribute
    );
    EFI_STATUS (*ClearScreen)(
        EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This
    );
    EFI_STATUS (*SetCursorPosition)(
        EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This,
        UINTN Column,
        UINTN Row
    );
    EFI_STATUS (*EnableCursor)(
        EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This,
        BOOLEAN Visible
    );
};

// EFI Boot Services
struct EFI_BOOT_SERVICES {
    char _pad1[24];  // EFI Table header (ignored)
    // Task Priority Services (ignored here)
    char _pad2[8];

    // Memory Services
    EFI_STATUS (*AllocatePages)(
        UINTN Type,
        UINTN MemoryType,
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
    EFI_STATUS (*AllocatePool)(
        UINTN PoolType,
        UINTN Size,
        VOID **Buffer
    );
    EFI_STATUS (*FreePool)(
        VOID *Buffer
    );

    // Event & Timer services (ignored here)
    char _pad3[24];

    // Protocol Handler Services (partial)
    EFI_STATUS (*HandleProtocol)(
        EFI_HANDLE Handle,
        const EFI_GUID *Protocol,
        VOID **Interface
    );
    char _pad4[64];

    // Image Services (ignored here)
    char _pad5[40];

    // Miscellaneous Services (ignored here)
    char _pad6[16];

    // Exit Boot Services
    EFI_STATUS (*ExitBootServices)(
        EFI_HANDLE ImageHandle,
        UINTN MapKey
    );

    // UEFI 2.0 Extensions (ignored here)
};

// Simple File System Protocol
struct EFI_SIMPLE_FILE_SYSTEM_PROTOCOL {
    UINT64 Revision;
    EFI_STATUS (*OpenVolume)(
        EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *This,
        EFI_FILE_PROTOCOL **Root
    );
};

// File Protocol (minimal subset)
struct EFI_FILE_PROTOCOL {
    UINT64 Revision;
    EFI_STATUS (*Open)(
        EFI_FILE_PROTOCOL *This,
        EFI_FILE_PROTOCOL **NewHandle,
        CHAR16 *FileName,
        UINT64 OpenMode,
        UINT64 Attributes
    );
    EFI_STATUS (*Close)(EFI_FILE_PROTOCOL *This);
    EFI_STATUS (*Read)(EFI_FILE_PROTOCOL *This, UINTN *BufferSize, VOID *Buffer);
};

#define EFI_FILE_MODE_READ 0x0000000000000001ULL

// EFI Memory Descriptor
struct EFI_MEMORY_DESCRIPTOR {
    UINT32                Type;
    UINT32                Pad;
    EFI_PHYSICAL_ADDRESS  PhysicalStart;
    EFI_VIRTUAL_ADDRESS   VirtualStart;
    UINT64                NumberOfPages;
    UINT64                Attribute;
};

// EFI System Table
typedef struct {
    char _pad[44];  // EFI table header (ignored)
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *ConOut;
    char _pad2[8];  // Ignore ConIn for simplicity
    EFI_BOOT_SERVICES *BootServices;
} EFI_SYSTEM_TABLE;

static const EFI_GUID gEfiSimpleFileSystemProtocolGuid = {
    0x964e5b21, 0x6459, 0x11d2,
    {0x8e, 0x39, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b}
};

#endif // EFI_H
