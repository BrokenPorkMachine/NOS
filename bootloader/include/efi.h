#ifndef EFI_H
#define EFI_H

// ----------------------------------------------------------------------
// 1. Typedefs FIRST (so all types are defined before use)
// ----------------------------------------------------------------------
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

// UEFI calling convention
#ifndef EFIAPI
#define EFIAPI
#endif

#ifndef NULL
#define NULL ((void *)0)
#endif

// ----------------------------------------------------------------------
// 2. Function Prototypes (no implementations in header!)
// ----------------------------------------------------------------------
VOID *EFIAPI CopyMem(VOID *Destination, const VOID *Source, UINTN Length);
VOID *EFIAPI SetMem(VOID *Buffer, UINTN Size, UINT8 Value);

// ----------------------------------------------------------------------
// 3. EFI GUID
// ----------------------------------------------------------------------
typedef struct {
    UINT32  Data1;
    UINT16  Data2;
    UINT16  Data3;
    UINT8   Data4[8];
} EFI_GUID;

// ----------------------------------------------------------------------
// 4. UEFI Status Codes, Memory Types, etc
// ----------------------------------------------------------------------
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

// ----------------------------------------------------------------------
// 5. EFI Memory Descriptor
// ----------------------------------------------------------------------
typedef struct {
    UINT32                Type;
    UINT32                Pad;
    EFI_PHYSICAL_ADDRESS  PhysicalStart;
    EFI_VIRTUAL_ADDRESS   VirtualStart;
    UINT64                NumberOfPages;
    UINT64                Attribute;
} EFI_MEMORY_DESCRIPTOR;

// ----------------------------------------------------------------------
// 6. Forward Declarations for Protocols
// ----------------------------------------------------------------------
typedef struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL;
typedef struct EFI_BOOT_SERVICES EFI_BOOT_SERVICES;
typedef struct EFI_FILE_PROTOCOL EFI_FILE_PROTOCOL;
typedef struct EFI_SIMPLE_FILE_SYSTEM_PROTOCOL EFI_SIMPLE_FILE_SYSTEM_PROTOCOL;

// ----------------------------------------------------------------------
// 7. Simple Text Output Protocol
// ----------------------------------------------------------------------
struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL {
    EFI_STATUS (*Reset)(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL*, BOOLEAN);
    EFI_STATUS (*OutputString)(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL*, CHAR16*);
    EFI_STATUS (*TestString)(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL*, CHAR16*);
    EFI_STATUS (*QueryMode)(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL*, UINTN, UINTN*, UINTN*);
    EFI_STATUS (*SetMode)(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL*, UINTN);
    EFI_STATUS (*SetAttribute)(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL*, UINTN);
    EFI_STATUS (*ClearScreen)(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL*);
    EFI_STATUS (*SetCursorPosition)(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL*, UINTN, UINTN);
    EFI_STATUS (*EnableCursor)(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL*, BOOLEAN);
};

// ----------------------------------------------------------------------
// 8. EFI Boot Services (partial, enough to boot)
// ----------------------------------------------------------------------
struct EFI_BOOT_SERVICES {
    char _pad1[24+8]; // skip headers and TPL services
    EFI_STATUS (*AllocatePages)(UINTN, UINTN, UINTN, EFI_PHYSICAL_ADDRESS*);
    EFI_STATUS (*FreePages)(EFI_PHYSICAL_ADDRESS, UINTN);
    EFI_STATUS (*GetMemoryMap)(UINTN*, EFI_MEMORY_DESCRIPTOR*, UINTN*, UINTN*, UINT32*);
    EFI_STATUS (*AllocatePool)(UINTN, UINTN, VOID**);
    EFI_STATUS (*FreePool)(VOID*);
    char _pad2[24]; // event/timers
    EFI_STATUS (*HandleProtocol)(EFI_HANDLE, EFI_GUID*, VOID**);
    char _pad3[120]; // misc/image services
    EFI_STATUS (*ExitBootServices)(EFI_HANDLE, UINTN);
};

// ----------------------------------------------------------------------
// 9. Simple File System Protocol
// ----------------------------------------------------------------------
struct EFI_SIMPLE_FILE_SYSTEM_PROTOCOL {
    UINT64 Revision;
    EFI_STATUS (*OpenVolume)(EFI_SIMPLE_FILE_SYSTEM_PROTOCOL*, EFI_FILE_PROTOCOL**);
};

// ----------------------------------------------------------------------
// 10. File Protocol (minimal subset)
// ----------------------------------------------------------------------
struct EFI_FILE_PROTOCOL {
    UINT64 Revision;
    EFI_STATUS (*Open)(EFI_FILE_PROTOCOL*, EFI_FILE_PROTOCOL**, CHAR16*, UINT64, UINT64);
    EFI_STATUS (*Close)(EFI_FILE_PROTOCOL*);
    EFI_STATUS (*Read)(EFI_FILE_PROTOCOL*, UINTN*, VOID*);
};

#define EFI_FILE_MODE_READ 0x0000000000000001ULL

// ----------------------------------------------------------------------
// 11. EFI System Table (minimal subset)
// ----------------------------------------------------------------------
typedef struct {
    char _pad[44];  // EFI table header (ignored)
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *ConOut;
    char _pad2[8];  // Ignore ConIn for simplicity
    EFI_BOOT_SERVICES *BootServices;
} EFI_SYSTEM_TABLE;

// ----------------------------------------------------------------------
// 12. GUID for EFI_SIMPLE_FILE_SYSTEM_PROTOCOL
// ----------------------------------------------------------------------
static const EFI_GUID gEfiSimpleFileSystemProtocolGuid = {
    0x964e5b21, 0x6459, 0x11d2,
    {0x8e, 0x39, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b}
};

#endif // EFI_H
// --- UEFI Graphics Output Protocol (GOP) ---

typedef struct {
    UINT32  Version;
    UINT32  HorizontalResolution;
    UINT32  VerticalResolution;
    UINT32  PixelFormat;
    UINT32  PixelsPerScanLine;
} EFI_GRAPHICS_OUTPUT_MODE_INFORMATION;

typedef struct {
    UINT32                         MaxMode;
    UINT32                         Mode;
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *Info;
    UINTN                          SizeOfInfo;
    EFI_PHYSICAL_ADDRESS           FrameBufferBase;
    UINTN                          FrameBufferSize;
} EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE;

typedef struct EFI_GRAPHICS_OUTPUT_PROTOCOL EFI_GRAPHICS_OUTPUT_PROTOCOL;
struct EFI_GRAPHICS_OUTPUT_PROTOCOL {
    EFI_STATUS (*QueryMode)(EFI_GRAPHICS_OUTPUT_PROTOCOL*, UINT32, UINTN*, EFI_GRAPHICS_OUTPUT_MODE_INFORMATION**);
    EFI_STATUS (*SetMode)(EFI_GRAPHICS_OUTPUT_PROTOCOL*, UINT32);
    // PixelBlt skipped
    EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE *Mode;
};

// The GOP GUID
static const EFI_GUID gEfiGraphicsOutputProtocolGuid = { 0x9042a9de,0x23dc,0x4a38,{0x96,0xfb,0x7a,0xde,0xd0,0x80,0x51,0x6a} };

// ACPI 2.0 Table GUID (for RSDP)
static const EFI_GUID gEfiAcpi20TableGuid = {0x8868e871,0xe4f1,0x11d3,{0xbc,0x22,0x00,0x80,0xc7,0x3c,0x88,0x81}};
