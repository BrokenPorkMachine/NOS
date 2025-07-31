#ifndef EFI_H
#define EFI_H

// Basic UEFI types
typedef unsigned char       UINT8;
typedef unsigned short      UINT16;
typedef unsigned int        UINT32;
typedef unsigned long long  UINT64;
typedef UINT64              UINTN;
typedef long long           INT64;
typedef unsigned short      CHAR16;
typedef void                VOID;
typedef UINT64              EFI_STATUS;
typedef VOID*               EFI_HANDLE;
typedef VOID*               EFI_EVENT;

// Status codes
#define EFI_SUCCESS         0
#define EFI_LOAD_ERROR      ((EFI_STATUS)0x8000000000000001)
#define EFI_INVALID_PARAMETER ((EFI_STATUS)0x8000000000000002)
// (add more as needed)

// GUID definition
typedef struct {
    UINT32  Data1;
    UINT16  Data2;
    UINT16  Data3;
    UINT8   Data4[8];
} EFI_GUID;

// Forward declarations for protocol structs
struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL;
struct EFI_SIMPLE_TEXT_INPUT_PROTOCOL;
struct EFI_BOOT_SERVICES;
struct EFI_RUNTIME_SERVICES;
struct EFI_SIMPLE_FILE_SYSTEM_PROTOCOL;
struct EFI_FILE_PROTOCOL;

// EFI_TABLE_HEADER structure
typedef struct {
    UINT64  Signature;
    UINT32  Revision;
    UINT32  HeaderSize;
    UINT32  CRC32;
    UINT32  Reserved;
} EFI_TABLE_HEADER;

// EFI_SYSTEM_TABLE structure
typedef struct {
    EFI_TABLE_HEADER                Hdr;
    CHAR16                          *FirmwareVendor;
    UINT32                          FirmwareRevision;
    EFI_HANDLE                      ConsoleInHandle;
    struct EFI_SIMPLE_TEXT_INPUT_PROTOCOL *ConIn;
    EFI_HANDLE                      ConsoleOutHandle;
    struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *ConOut;
    EFI_HANDLE                      StandardErrorHandle;
    struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *StdErr;
    struct EFI_RUNTIME_SERVICES     *RuntimeServices;
    struct EFI_BOOT_SERVICES        *BootServices;
    UINTN                           NumberOfTableEntries;
    VOID                            *ConfigurationTable;
} EFI_SYSTEM_TABLE;

// EFI_BOOT_SERVICES structure (truncated to essentials for bootloader use)
typedef struct EFI_BOOT_SERVICES {
    EFI_TABLE_HEADER        Hdr;

    // Task Priority Services
    VOID *RaiseTPL;
    VOID *RestoreTPL;

    // Memory Services
    EFI_STATUS (*AllocatePages)(
        UINTN Type,
        UINTN MemoryType,
        UINTN Pages,
        UINT64 *Memory
    );
    EFI_STATUS (*FreePages)(
        UINT64 Memory,
        UINTN Pages
    );
    EFI_STATUS (*GetMemoryMap)(
        UINTN *MemoryMapSize,
        VOID *MemoryMap,
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

    // ... (other members skipped)
} EFI_BOOT_SERVICES;

// Memory allocation types
#define AllocateAnyPages     0
#define AllocateMaxAddress   1
#define AllocateAddress      2

// Memory types
#define EfiLoaderData        4

// File protocol and related
typedef struct EFI_FILE_PROTOCOL EFI_FILE_PROTOCOL;
typedef struct EFI_SIMPLE_FILE_SYSTEM_PROTOCOL EFI_SIMPLE_FILE_SYSTEM_PROTOCOL;

// EFI_FILE_PROTOCOL structure (simplified)
struct EFI_FILE_PROTOCOL {
    UINT64 Revision;
    EFI_STATUS (*Open)(
        EFI_FILE_PROTOCOL *This,
        EFI_FILE_PROTOCOL **NewHandle,
        CHAR16 *FileName,
        UINT64 OpenMode,
        UINT64 Attributes
    );
    EFI_STATUS (*Close)(
        EFI_FILE_PROTOCOL *This
    );
    EFI_STATUS (*Delete)(
        EFI_FILE_PROTOCOL *This
    );
    EFI_STATUS (*Read)(
        EFI_FILE_PROTOCOL *This,
        UINTN *BufferSize,
        VOID *Buffer
    );
    EFI_STATUS (*Write)(
        EFI_FILE_PROTOCOL *This,
        UINTN *BufferSize,
        VOID *Buffer
    );
    EFI_STATUS (*GetPosition)(
        EFI_FILE_PROTOCOL *This,
        UINT64 *Position
    );
    EFI_STATUS (*SetPosition)(
        EFI_FILE_PROTOCOL *This,
        UINT64 Position
    );
    EFI_STATUS (*GetInfo)(
        EFI_FILE_PROTOCOL *This,
        EFI_GUID *InformationType,
        UINTN *BufferSize,
        VOID *Buffer
    );
    EFI_STATUS (*SetInfo)(
        EFI_FILE_PROTOCOL *This,
        EFI_GUID *InformationType,
        UINTN BufferSize,
        VOID *Buffer
    );
    EFI_STATUS (*Flush)(
        EFI_FILE_PROTOCOL *This
    );
    // ... (add more if you need)
};

// EFI_SIMPLE_FILE_SYSTEM_PROTOCOL structure
struct EFI_SIMPLE_FILE_SYSTEM_PROTOCOL {
    UINT64 Revision;
    EFI_STATUS (*OpenVolume)(
        EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *This,
        EFI_FILE_PROTOCOL **Root
    );
};

// Text Output protocol (simplified)
struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL {
    VOID *Reset;
    EFI_STATUS (*OutputString)(
        struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This,
        const CHAR16 *String
    );
    // ... (other members skipped)
};

// Text Input protocol (placeholder)
struct EFI_SIMPLE_TEXT_INPUT_PROTOCOL {
    // ... (add as needed)
};

#endif // EFI_H
