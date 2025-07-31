#ifndef EFI_H
#define EFI_H

// ABI for UEFI on x86_64
#define EFIAPI __attribute__((ms_abi))

// Basic UEFI typedefs
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

// EFI GUID type
typedef struct {
    UINT32  Data1;
    UINT16  Data2;
    UINT16  Data3;
    UINT8   Data4[8];
} EFI_GUID;

// EFI Status Codes
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

// EFI Memory Descriptor
typedef struct {
    UINT32                Type;
    UINT32                Pad;
    EFI_PHYSICAL_ADDRESS  PhysicalStart;
    EFI_VIRTUAL_ADDRESS   VirtualStart;
    UINT64                NumberOfPages;
    UINT64                Attribute;
} EFI_MEMORY_DESCRIPTOR;

// Forward declarations
typedef struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL;
typedef struct EFI_BOOT_SERVICES EFI_BOOT_SERVICES;
typedef struct EFI_FILE_PROTOCOL EFI_FILE_PROTOCOL;
typedef struct EFI_SIMPLE_FILE_SYSTEM_PROTOCOL EFI_SIMPLE_FILE_SYSTEM_PROTOCOL;

// EFI Simple Text Output Protocol
struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL {
    EFI_STATUS (EFIAPI *Reset)(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL*, BOOLEAN);
    EFI_STATUS (EFIAPI *OutputString)(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL*, CHAR16*);
    EFI_STATUS (EFIAPI *TestString)(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL*, CHAR16*);
    EFI_STATUS (EFIAPI *QueryMode)(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL*, UINTN, UINTN*, UINTN*);
    EFI_STATUS (EFIAPI *SetMode)(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL*, UINTN);
    EFI_STATUS (EFIAPI *SetAttribute)(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL*, UINTN);
    EFI_STATUS (EFIAPI *ClearScreen)(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL*);
    EFI_STATUS (EFIAPI *SetCursorPosition)(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL*, UINTN, UINTN);
    EFI_STATUS (EFIAPI *EnableCursor)(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL*, BOOLEAN);
};

// EFI Boot Services (partial)
struct EFI_BOOT_SERVICES {
    char _pad1[32]; // skip headers and TPL
    EFI_STATUS (EFIAPI *AllocatePages)(UINTN, UINTN, UINTN, EFI_PHYSICAL_ADDRESS*);
    EFI_STATUS (EFIAPI *FreePages)(EFI_PHYSICAL_ADDRESS, UINTN);
    EFI_STATUS (EFIAPI *GetMemoryMap)(UINTN*, EFI_MEMORY_DESCRIPTOR*, UINTN*, UINTN*, UINT32*);
    EFI_STATUS (EFIAPI *AllocatePool)(UINTN, UINTN, VOID**);
    EFI_STATUS (EFIAPI *FreePool)(VOID*);
    char _pad2[24];
    EFI_STATUS (EFIAPI *HandleProtocol)(EFI_HANDLE, EFI_GUID*, VOID**);
    char _pad3[120];
    EFI_STATUS (EFIAPI *ExitBootServices)(EFI_HANDLE, UINTN);
};

// Simple File System Protocol
struct EFI_SIMPLE_FILE_SYSTEM_PROTOCOL {
    UINT64 Revision;
    EFI_STATUS (EFIAPI *OpenVolume)(EFI_SIMPLE_FILE_SYSTEM_PROTOCOL*, EFI_FILE_PROTOCOL**);
};

// File Protocol (minimal subset)
struct EFI_FILE_PROTOCOL {
    UINT64 Revision;
    EFI_STATUS (EFIAPI *Open)(EFI_FILE_PROTOCOL*, EFI_FILE_PROTOCOL**, CHAR16*, UINT64, UINT64);
    EFI_STATUS (EFIAPI *Close)(EFI_FILE_PROTOCOL*);
    EFI_STATUS (EFIAPI *Read)(EFI_FILE_PROTOCOL*, UINTN*, VOID*);
};

#define EFI_FILE_MODE_READ 0x0000000000000001ULL

// EFI System Table (minimal)
typedef struct {
    char _pad[44];
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *ConOut;
    char _pad2[8];
    EFI_BOOT_SERVICES *BootServices;
} EFI_SYSTEM_TABLE;

// GUID for EFI_SIMPLE_FILE_SYSTEM_PROTOCOL
static const EFI_GUID gEfiSimpleFileSystemProtocolGuid = {
    0x964e5b21, 0x6459, 0x11d2,
    {0x8e, 0x39, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b}
};

// Memory helpers (implemented in NitrOBoot.c, not in UEFI)
static inline VOID *EFIAPI CopyMem(VOID *Destination, const VOID *Source, UINTN Length) {
    UINT8 *dst = (UINT8*)Destination;
    const UINT8 *src = (const UINT8*)Source;
    for (UINTN i = 0; i < Length; ++i) dst[i] = src[i];
    return Destination;
}
static inline VOID *EFIAPI SetMem(VOID *Buffer, UINTN Size, UINT8 Value) {
    UINT8 *p = (UINT8*)Buffer;
    for (UINTN i = 0; i < Size; ++i) p[i] = Value;
    return Buffer;
}

#endif // EFI_H
