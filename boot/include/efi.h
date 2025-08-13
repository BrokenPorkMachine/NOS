#ifndef EFI_H
#define EFI_H

#include <stddef.h> // for NULL

#ifdef __GNUC__
#define EFIAPI __attribute__((ms_abi))
#else
#define EFIAPI
#endif

#define EFI_FILE_MODE_READ      0x0000000000000001ULL
#define EFI_FILE_DIRECTORY      0x0000000000000010ULL

#define EFI_ERROR(x)            ((x) != EFI_SUCCESS)
#define EFI_SECURITY_VIOLATION  ((EFI_STATUS)0x800000000000001e)

// ====================
// Basic UEFI Types
// ====================
typedef unsigned char       UINT8;
typedef unsigned short      UINT16;
typedef unsigned int        UINT32;
typedef unsigned long long  UINT64;
typedef UINT64              UINTN;
typedef UINT64              EFI_STATUS;
typedef UINT64              EFI_PHYSICAL_ADDRESS;
typedef UINT16              CHAR16;
typedef void                VOID;
typedef VOID*               EFI_HANDLE;
typedef signed short        INT16;
typedef unsigned char       BOOLEAN;
#define TRUE 1
#define FALSE 0
typedef VOID*               EFI_EVENT;

// ====================
// Status Codes (Common)
// ====================
#define EFI_SUCCESS              0
#define EFI_LOAD_ERROR           ((EFI_STATUS)0x8000000000000001)
#define EFI_INVALID_PARAMETER    ((EFI_STATUS)0x8000000000000002)
#define EFI_BUFFER_TOO_SMALL     ((EFI_STATUS)0x8000000000000005)
// Add others as needed

// ====================
// GUID Structure
// ====================
typedef struct {
    UINT32  Data1;
    UINT16  Data2;
    UINT16  Data3;
    UINT8   Data4[8];
} EFI_GUID;

typedef struct {
    UINT16 Year;
    UINT8  Month;
    UINT8  Day;
    UINT8  Hour;
    UINT8  Minute;
    UINT8  Second;
    UINT8  Pad1;
    UINT32 Nanosecond;
    INT16  TimeZone;
    UINT8  Daylight;
    UINT8  Pad2;
} EFI_TIME;

typedef struct {
    UINT64   Size;
    UINT64   FileSize;
    UINT64   PhysicalSize;
    EFI_TIME CreateTime;
    EFI_TIME LastAccessTime;
    EFI_TIME ModificationTime;
    UINT64   Attribute;
    CHAR16   FileName[1];
} EFI_FILE_INFO;
// GOP structures
typedef struct {
    UINT32                       Version;
    UINT32                       HorizontalResolution;
    UINT32                       VerticalResolution;
    UINT32                       PixelFormat;
    UINT32                       PixelInformation[4];
    UINT32                       PixelsPerScanLine;
} EFI_GRAPHICS_OUTPUT_MODE_INFORMATION;

typedef struct {
    UINT32                                MaxMode;
    UINT32                                Mode;
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION* Info;
    UINTN                                 SizeOfInfo;
    UINT64                                FrameBufferBase;
    UINTN                                 FrameBufferSize;
} EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE;

typedef struct EFI_GRAPHICS_OUTPUT_PROTOCOL EFI_GRAPHICS_OUTPUT_PROTOCOL;
struct EFI_GRAPHICS_OUTPUT_PROTOCOL {
    EFI_STATUS (*QueryMode)(
        EFI_GRAPHICS_OUTPUT_PROTOCOL*,
        UINT32,
        UINTN*,
        EFI_GRAPHICS_OUTPUT_MODE_INFORMATION**
    );
    EFI_STATUS (*SetMode)(EFI_GRAPHICS_OUTPUT_PROTOCOL*, UINT32);
    EFI_STATUS (*Blt)(EFI_GRAPHICS_OUTPUT_PROTOCOL*, VOID*, UINT32, UINTN, UINTN, UINTN, UINTN, UINTN, UINTN, UINTN);
    EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE* Mode;
};

// ====================
// Forward Declarations
// ====================
struct EFI_SYSTEM_TABLE; typedef struct EFI_SYSTEM_TABLE EFI_SYSTEM_TABLE;
typedef struct EFI_CONFIGURATION_TABLE EFI_CONFIGURATION_TABLE;
struct EFI_BOOT_SERVICES; typedef struct EFI_BOOT_SERVICES EFI_BOOT_SERVICES;
struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL; typedef struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL;
struct EFI_SIMPLE_TEXT_INPUT_PROTOCOL; typedef struct EFI_SIMPLE_TEXT_INPUT_PROTOCOL EFI_SIMPLE_TEXT_INPUT_PROTOCOL;
struct EFI_SIMPLE_FILE_SYSTEM_PROTOCOL; typedef struct EFI_SIMPLE_FILE_SYSTEM_PROTOCOL EFI_SIMPLE_FILE_SYSTEM_PROTOCOL;
struct EFI_FILE_PROTOCOL; typedef struct EFI_FILE_PROTOCOL EFI_FILE_PROTOCOL;
typedef struct EFI_MEMORY_DESCRIPTOR EFI_MEMORY_DESCRIPTOR;

typedef struct {
    UINT16 ScanCode;
    CHAR16 UnicodeChar;
} EFI_INPUT_KEY;

typedef EFI_STATUS (EFIAPI *EFI_INPUT_RESET)(EFI_SIMPLE_TEXT_INPUT_PROTOCOL*, BOOLEAN);
typedef EFI_STATUS (EFIAPI *EFI_INPUT_READ_KEY)(EFI_SIMPLE_TEXT_INPUT_PROTOCOL*, EFI_INPUT_KEY*);
struct EFI_SIMPLE_TEXT_INPUT_PROTOCOL {
    EFI_INPUT_RESET    Reset;
    EFI_INPUT_READ_KEY ReadKeyStroke;
    EFI_EVENT          WaitForKey;
};

// ====================
// EFI_TABLE_HEADER
// ====================
typedef struct {
    UINT64  Signature;
    UINT32  Revision;
    UINT32  HeaderSize;
    UINT32  CRC32;
    UINT32  Reserved;
} EFI_TABLE_HEADER;

// ====================
// System Table
// ====================
struct EFI_SYSTEM_TABLE {
    EFI_TABLE_HEADER                          Hdr;
    CHAR16*                                   FirmwareVendor;
    UINT32                                    FirmwareRevision;
    EFI_HANDLE                                ConsoleInHandle;
    struct EFI_SIMPLE_TEXT_INPUT_PROTOCOL*    ConIn;
    EFI_HANDLE                                ConsoleOutHandle;
    struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL*   ConOut;
    EFI_HANDLE                                StandardErrorHandle;
    struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL*   StdErr;
    struct EFI_RUNTIME_SERVICES*              RuntimeServices;
    struct EFI_BOOT_SERVICES*                 BootServices;
    UINTN                                     NumberOfTableEntries;
    EFI_CONFIGURATION_TABLE*                  ConfigurationTable;
};

struct EFI_CONFIGURATION_TABLE {
    EFI_GUID VendorGuid;
    VOID    *VendorTable;
};

// ====================
// Runtime Services table
// ====================
struct EFI_RUNTIME_SERVICES {
    EFI_TABLE_HEADER Hdr;

    /* Time Services */
    EFI_STATUS (*GetTime)(EFI_TIME*, UINT32*);
    EFI_STATUS (*SetTime)(EFI_TIME*);
    EFI_STATUS (*GetWakeupTime)(UINT8*, UINT8*, EFI_TIME*);
    EFI_STATUS (*SetWakeupTime)(UINT8, UINT8, EFI_TIME*);

    /* Virtual Memory Services */
    EFI_STATUS (*SetVirtualAddressMap)(UINTN, UINTN, UINT32, EFI_MEMORY_DESCRIPTOR*);
    EFI_STATUS (*ConvertPointer)(UINTN, VOID**);

    /* Variable Services */
    EFI_STATUS (*GetVariable)(CHAR16*, EFI_GUID*, UINT32*, UINTN*, VOID*);
    EFI_STATUS (*GetNextVariableName)(UINTN*, CHAR16*, EFI_GUID*);
    EFI_STATUS (*SetVariable)(CHAR16*, EFI_GUID*, UINT32, UINTN, VOID*);

    /* Miscellaneous Services */
    EFI_STATUS (*GetNextHighMonotonicCount)(UINT32*);
    VOID (*ResetSystem)(UINT32, EFI_STATUS, UINTN, VOID*);
    EFI_STATUS (*UpdateCapsule)(VOID**, UINTN, EFI_PHYSICAL_ADDRESS);
    EFI_STATUS (*QueryCapsuleCapabilities)(VOID**, UINTN, UINT64*, UINT32*);
    EFI_STATUS (*QueryVariableInfo)(UINT32, UINT64*, UINT64*, UINT64*);
};

// ====================
// Boot Services table (ordered as in UEFI spec)
// The structure layout must match the real UEFI table so that
// function pointers retrieved from it are correct.  Many entries are
// unused by the bootloader, so they are typed as generic pointers but
// still occupy space to preserve the correct offsets.
// Only the functions actually used are given explicit prototypes.
// ====================
struct EFI_BOOT_SERVICES {
    EFI_TABLE_HEADER  Hdr;

    /* Task Priority Services */
    VOID *RaiseTPL;
    VOID *RestoreTPL;

    /* Memory Services */
    EFI_STATUS (*AllocatePages)(UINTN Type, UINTN MemoryType, UINTN Pages,
                                EFI_PHYSICAL_ADDRESS *Memory);
    EFI_STATUS (*FreePages)(EFI_PHYSICAL_ADDRESS Memory, UINTN Pages);
    EFI_STATUS (*GetMemoryMap)(UINTN *MemoryMapSize, VOID *MemoryMap,
                               UINTN *MapKey, UINTN *DescriptorSize,
                               UINT32 *DescriptorVersion);
    EFI_STATUS (*AllocatePool)(UINTN PoolType, UINTN Size, VOID **Buffer);
    EFI_STATUS (*FreePool)(VOID *Buffer);

    /* Event & Timer Services */
    VOID *CreateEvent;
    VOID *SetTimer;
    VOID *WaitForEvent;
    VOID *SignalEvent;
    VOID *CloseEvent;
    VOID *CheckEvent;

    /* Protocol Handler Services */
    VOID *InstallProtocolInterface;
    VOID *ReinstallProtocolInterface;
    VOID *UninstallProtocolInterface;
    EFI_STATUS (*HandleProtocol)(EFI_HANDLE Handle, EFI_GUID *Protocol,
                                VOID **Interface);
    VOID *Reserved;
    VOID *RegisterProtocolNotify;
    VOID *LocateHandle;
    VOID *LocateDevicePath;
    VOID *InstallConfigurationTable;

    /* Image Services */
    VOID *LoadImage;
    VOID *StartImage;
    VOID *Exit;
    VOID *UnloadImage;
    EFI_STATUS (*ExitBootServices)(EFI_HANDLE ImageHandle, UINTN MapKey);

    /* Misc Services */
    VOID *GetNextMonotonicCount;
    VOID *Stall;
    VOID *SetWatchdogTimer;

    /* Driver Support Services */
    VOID *ConnectController;
    VOID *DisconnectController;

    /* Open and Close Protocol Services */
    VOID *OpenProtocol;
    VOID *CloseProtocol;
    VOID *OpenProtocolInformation;

    /* Library Services */
    VOID *ProtocolsPerHandle;
    VOID *LocateHandleBuffer;
    EFI_STATUS (*LocateProtocol)(EFI_GUID *Protocol, VOID *Registration,
                                VOID **Interface);
    VOID *InstallMultipleProtocolInterfaces;
    VOID *UninstallMultipleProtocolInterfaces;

    /* 32-bit CRC Services */
    VOID *CalculateCrc32;

    /* Memory Utility Services */
    VOID *CopyMem;
    VOID *SetMem;

    /* UEFI 2.0 Capsule Services */
    VOID *CreateEventEx;
};

// ====================
// Allocation and Memory Type Macros
// ====================
#define EFI_ALLOCATE_ANY_PAGES    0
#define EFI_ALLOCATE_MAX_ADDRESS  1
#define EFI_ALLOCATE_ADDRESS      2

#define AllocateAnyPages          0
#define AllocateMaxAddress        1
#define AllocateAddress           2

#define EfiLoaderCode             3
#define EfiLoaderData             4

// ====================
// EFI_FILE_PROTOCOL
// ====================
struct EFI_FILE_PROTOCOL {
    UINT64 Revision;
    EFI_STATUS (*Open)(
        struct EFI_FILE_PROTOCOL* This,
        struct EFI_FILE_PROTOCOL** NewHandle,
        CHAR16* FileName,
        UINT64 OpenMode,
        UINT64 Attributes
    );
    EFI_STATUS (*Close)(struct EFI_FILE_PROTOCOL* This);
    EFI_STATUS (*Delete)(struct EFI_FILE_PROTOCOL* This);
    EFI_STATUS (*Read)(
        struct EFI_FILE_PROTOCOL* This,
        UINTN* BufferSize,
        VOID* Buffer
    );
    EFI_STATUS (*Write)(
        struct EFI_FILE_PROTOCOL* This,
        UINTN* BufferSize,
        VOID* Buffer
    );
    EFI_STATUS (*GetPosition)(struct EFI_FILE_PROTOCOL* This, UINT64* Position);
    EFI_STATUS (*SetPosition)(struct EFI_FILE_PROTOCOL* This, UINT64 Position);
    EFI_STATUS (*GetInfo)(
        struct EFI_FILE_PROTOCOL* This,
        EFI_GUID* InformationType,
        UINTN* BufferSize,
        VOID* Buffer
    );
    EFI_STATUS (*SetInfo)(
        struct EFI_FILE_PROTOCOL* This,
        EFI_GUID* InformationType,
        UINTN BufferSize,
        VOID* Buffer
    );
    EFI_STATUS (*Flush)(struct EFI_FILE_PROTOCOL* This);
    // Add others as needed
};

// ====================
// EFI_SIMPLE_FILE_SYSTEM_PROTOCOL
// ====================
struct EFI_SIMPLE_FILE_SYSTEM_PROTOCOL {
    UINT64 Revision;
    EFI_STATUS (*OpenVolume)(
        struct EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* This,
        struct EFI_FILE_PROTOCOL** Root
    );
};

// ====================
// EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL
// ====================
struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL {
    EFI_STATUS (*Reset)(
        struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL* This,
        UINT8 ExtendedVerification
    );
    EFI_STATUS (*OutputString)(
        struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL* This,
        const CHAR16* String
    );
    EFI_STATUS (*TestString)(
        struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL* This,
        const CHAR16* String
    );
    EFI_STATUS (*QueryMode)(
        struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL* This,
        UINTN ModeNumber,
        UINTN* Columns,
        UINTN* Rows
    );
    EFI_STATUS (*SetMode)(
        struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL* This,
        UINTN ModeNumber
    );
    EFI_STATUS (*SetAttribute)(
        struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL* This,
        UINTN Attribute
    );
    EFI_STATUS (*ClearScreen)(
        struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL* This
    );
    EFI_STATUS (*SetCursorPosition)(
        struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL* This,
        UINTN Column,
        UINTN Row
    );
    EFI_STATUS (*EnableCursor)(
        struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL* This,
        UINT8 Visible
    );
    // Add others as needed
};

// ====================
// Text Color Macros
// ====================
#define EFI_BLACK         0x00
#define EFI_BLUE          0x01
#define EFI_GREEN         0x02
#define EFI_CYAN          0x03
#define EFI_RED           0x04
#define EFI_MAGENTA       0x05
#define EFI_BROWN         0x06
#define EFI_LIGHTGRAY     0x07
#define EFI_BRIGHT        0x08
#define EFI_DARKGRAY      0x08
#define EFI_LIGHTBLUE     0x09
#define EFI_LIGHTGREEN    0x0A
#define EFI_LIGHTCYAN     0x0B
#define EFI_LIGHTRED      0x0C
#define EFI_LIGHTMAGENTA  0x0D
#define EFI_YELLOW        0x0E
#define EFI_WHITE         0x0F

#define EFI_TEXT_ATTR(Foreground, Background) (((Foreground) & 0x0F) | (((Background) & 0x0F) << 4))
static const EFI_GUID gEfiGraphicsOutputProtocolGuid =
    { 0x9042a9de, 0x23dc, 0x4a38,
      { 0x96, 0xfb, 0x7a, 0xde, 0xd0, 0x80, 0x51, 0x6a } };

static const EFI_GUID gEfiAcpi20TableGuid =
    { 0x8868e871, 0xe4f1, 0x11d3, { 0xbc, 0x22, 0x0, 0x80, 0xc7, 0x3c, 0x88, 0x81 } };

static const EFI_GUID gEfiAcpi10TableGuid =
    { 0xeb9d2d30, 0x2d88, 0x11d3, { 0x9a, 0x16, 0x0, 0x0, 0x9c, 0x00, 0x83, 0x0b } };

static const EFI_GUID gEfiSimpleFileSystemProtocolGuid =
    { 0x964e5b22, 0x6459, 0x11d2, { 0x8e, 0x39, 0x0, 0xa0, 0xc9, 0x69, 0x72, 0x3b } };

static const EFI_GUID gEfiLoadedImageProtocolGuid =
    { 0x5b1b31a1, 0x9562, 0x11d2, { 0x8e, 0x3f, 0x0, 0xa0, 0xc9, 0x69, 0x72, 0x3b } };

static const EFI_GUID gEfiFileInfoGuid =
    { 0x09576e92, 0x6d3f, 0x11d2, { 0x8e, 0x39, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b } };

static const EFI_GUID gEfiGlobalVariableGuid =
    { 0x8be4df61, 0x93ca, 0x11d2,
      { 0xaa, 0x0d, 0x00, 0xe0, 0x98, 0x03, 0x2b, 0x8c } };

typedef struct EFI_LOADED_IMAGE_PROTOCOL {
    UINT32     Revision;
    EFI_HANDLE ParentHandle;
    struct EFI_SYSTEM_TABLE *SystemTable;
    EFI_HANDLE DeviceHandle;
    VOID      *FilePath;
    VOID      *Reserved;
    UINT32     LoadOptionsSize;
    VOID      *LoadOptions;
} EFI_LOADED_IMAGE_PROTOCOL;

// ====================
// EFI_MEMORY_DESCRIPTOR
// ====================
struct EFI_MEMORY_DESCRIPTOR {
    UINT32                Type;
    EFI_PHYSICAL_ADDRESS  PhysicalStart;
    EFI_PHYSICAL_ADDRESS  VirtualStart;
    UINT64                NumberOfPages;
    UINT64                Attribute;
};
typedef struct EFI_MEMORY_DESCRIPTOR EFI_MEMORY_DESCRIPTOR;

// ====================
// NULL Definition (if not included from stddef.h)
// ====================
#ifndef NULL
#define NULL ((void*)0)
#endif

#endif // EFI_H
