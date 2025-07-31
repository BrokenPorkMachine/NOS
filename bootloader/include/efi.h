#ifndef EFI_H
#define EFI_H

// --- Basic types ---
typedef unsigned char       UINT8;
typedef unsigned short      UINT16;
typedef unsigned int        UINT32;
typedef unsigned long long  UINT64;
typedef UINT64              UINTN;
typedef unsigned short      CHAR16;
typedef UINTN               EFI_STATUS;
typedef void*               VOID;
typedef VOID*               EFI_HANDLE;
typedef UINT64              EFI_PHYSICAL_ADDRESS;
typedef UINT64              EFI_VIRTUAL_ADDRESS;

#ifndef EFIAPI
#define EFIAPI
#endif

#ifndef NULL
#define NULL ((void *)0)
#endif

// --- Status codes ---
#define EFI_SUCCESS               0
#define EFI_LOAD_ERROR            (EFI_STATUS)(1ULL | (1ULL << 63))
#define EFI_OUT_OF_RESOURCES      (EFI_STATUS)(9ULL | (1ULL << 63))
#define EFI_INVALID_PARAMETER     (EFI_STATUS)(2ULL | (1ULL << 63))

// --- EFI GUID struct ---
typedef struct {
    UINT32  Data1;
    UINT16  Data2;
    UINT16  Data3;
    UINT8   Data4[8];
} EFI_GUID;

// --- Memory descriptor ---
typedef struct {
    UINT32                Type;
    UINT32                Pad;
    EFI_PHYSICAL_ADDRESS  PhysicalStart;
    EFI_VIRTUAL_ADDRESS   VirtualStart;
    UINT64                NumberOfPages;
    UINT64                Attribute;
} EFI_MEMORY_DESCRIPTOR;

// --- Text output protocol ---
typedef struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL;
struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL {
    EFI_STATUS (*Reset)(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL*, UINT8);
    EFI_STATUS (*OutputString)(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL*, CHAR16*);
    EFI_STATUS (*SetAttribute)(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL*, UINTN);
    EFI_STATUS (*ClearScreen)(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL*);
};

// --- EFI Boot Services (subset, ordered to match spec layout) ---
typedef struct EFI_BOOT_SERVICES EFI_BOOT_SERVICES;
struct EFI_BOOT_SERVICES {
    char _pad1[32];
    EFI_STATUS (*AllocatePages)(UINTN, UINTN, UINTN, EFI_PHYSICAL_ADDRESS*);
    EFI_STATUS (*FreePages)(EFI_PHYSICAL_ADDRESS, UINTN);
    EFI_STATUS (*GetMemoryMap)(UINTN*, EFI_MEMORY_DESCRIPTOR*, UINTN*, UINTN*, UINT32*);
    EFI_STATUS (*AllocatePool)(UINTN, UINTN, VOID**);
    EFI_STATUS (*FreePool)(VOID*);
    char _pad2[24];
    EFI_STATUS (*HandleProtocol)(EFI_HANDLE, EFI_GUID*, VOID**);
    EFI_STATUS (*LocateProtocol)(EFI_GUID*, VOID*, VOID**);
    char _pad3[112];
    EFI_STATUS (*ExitBootServices)(EFI_HANDLE, UINTN);
};

// --- EFI System Table (subset) ---
typedef struct {
    char _pad[44];
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *ConOut;
    char _pad2[8];
    EFI_BOOT_SERVICES *BootServices;
} EFI_SYSTEM_TABLE;

// --- Simple File System Protocol ---
typedef struct EFI_SIMPLE_FILE_SYSTEM_PROTOCOL EFI_SIMPLE_FILE_SYSTEM_PROTOCOL;
struct EFI_SIMPLE_FILE_SYSTEM_PROTOCOL {
    UINT64 Revision;
    EFI_STATUS (*OpenVolume)(EFI_SIMPLE_FILE_SYSTEM_PROTOCOL*, struct EFI_FILE_PROTOCOL**);
};

// --- File protocol (includes SetPosition etc) ---
typedef struct EFI_FILE_PROTOCOL EFI_FILE_PROTOCOL;
struct EFI_FILE_PROTOCOL {
    UINT64 Revision;
    EFI_STATUS (*Open)(EFI_FILE_PROTOCOL*, EFI_FILE_PROTOCOL**, CHAR16*, UINT64, UINT64);
    EFI_STATUS (*Close)(EFI_FILE_PROTOCOL*);
    EFI_STATUS (*Delete)(EFI_FILE_PROTOCOL*);
    EFI_STATUS (*Read)(EFI_FILE_PROTOCOL*, UINTN*, VOID*);
    EFI_STATUS (*Write)(EFI_FILE_PROTOCOL*, UINTN*, VOID*);
    EFI_STATUS (*GetPosition)(EFI_FILE_PROTOCOL*, UINT64*);
    EFI_STATUS (*SetPosition)(EFI_FILE_PROTOCOL*, UINT64);
    // ... Add more as needed, these suffice for most loaders.
};

#define EFI_FILE_MODE_READ   0x0000000000000001ULL
#define EFI_FILE_MODE_WRITE  0x0000000000000002ULL
#define EFI_FILE_MODE_CREATE 0x8000000000000000ULL

// --- Graphics Output Protocol (minimal) ---
typedef struct EFI_GRAPHICS_OUTPUT_PROTOCOL EFI_GRAPHICS_OUTPUT_PROTOCOL;

typedef struct {
    UINT32  Version;
    UINT32  HorizontalResolution;
    UINT32  VerticalResolution;
    UINT32  PixelFormat;
    UINT32  PixelsPerScanLine;
} EFI_GRAPHICS_OUTPUT_MODE_INFORMATION;

typedef struct {
    UINT32 MaxMode;
    UINT32 Mode;
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *Info;
    UINTN  SizeOfInfo;
    EFI_PHYSICAL_ADDRESS FrameBufferBase;
    UINTN FrameBufferSize;
} EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE;

struct EFI_GRAPHICS_OUTPUT_PROTOCOL {
    EFI_STATUS (*QueryMode)(EFI_GRAPHICS_OUTPUT_PROTOCOL*, UINT32, UINTN*, EFI_GRAPHICS_OUTPUT_MODE_INFORMATION**);
    EFI_STATUS (*SetMode)(EFI_GRAPHICS_OUTPUT_PROTOCOL*, UINT32);
    EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE *Mode;
};

// --- GUIDs for protocol lookup ---
static const EFI_GUID gEfiSimpleFileSystemProtocolGuid = {
    0x964e5b21, 0x6459, 0x11d2, {0x8e, 0x39, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b}
};
static const EFI_GUID gEfiGraphicsOutputProtocolGuid = {
    0x9042a9de, 0x23dc, 0x4a38, {0x96, 0xfb, 0x7a, 0xde, 0xd0, 0x80, 0x51, 0x6a}
};
static const EFI_GUID gEfiAcpi20TableGuid = {
    0x8868e871, 0xe4f1, 0x11d3, {0xbc, 0x22, 0x00, 0x80, 0xc7, 0x3c, 0x88, 0x81}
};

// --- Memory utilities (you must implement these in your .c) ---
VOID *EFIAPI CopyMem(VOID *Destination, const VOID *Source, UINTN Length);
VOID *EFIAPI SetMem(VOID *Buffer, UINTN Size, UINT8 Value);

#endif // EFI_H
