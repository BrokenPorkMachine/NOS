#ifndef EFI_H
#define EFI_H

#include <stddef.h> // For NULL

// Basic types
typedef unsigned char       UINT8;
typedef unsigned short      UINT16;
typedef unsigned int        UINT32;
typedef unsigned long long  UINT64;
typedef UINT64              UINTN;
typedef UINT64              EFI_STATUS;
typedef UINT64              EFI_PHYSICAL_ADDRESS;
typedef void                VOID;
typedef UINT16              CHAR16;
typedef VOID*               EFI_HANDLE;

// Status codes
#define EFI_SUCCESS         0

// Forward declarations
struct EFI_SYSTEM_TABLE;
struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL;
struct EFI_BOOT_SERVICES;
struct EFI_SIMPLE_FILE_SYSTEM_PROTOCOL;
struct EFI_FILE_PROTOCOL;

// EFI_GUID
typedef struct {
    UINT32  Data1;
    UINT16  Data2;
    UINT16  Data3;
    UINT8   Data4[8];
} EFI_GUID;

// EFI_SYSTEM_TABLE
struct EFI_SYSTEM_TABLE {
    char _pad1[44]; // not used in bootloader, pad to ConOut
    struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *ConOut;
    // add more as needed
    struct EFI_BOOT_SERVICES *BootServices;
};

// EFI_BOOT_SERVICES (minimal)
struct EFI_BOOT_SERVICES {
    char _pad2[24]; // skip to AllocatePages
    EFI_STATUS (*AllocatePages)(
        UINTN Type,
        UINTN MemoryType,
        UINTN Pages,
        EFI_PHYSICAL_ADDRESS *Memory
    );
    // ... (add more if needed)
};

// Allocation types
#define AllocateAnyPages       0
#define AllocateMaxAddress     1
#define AllocateAddress        2

// Memory types
#define EfiLoaderData          4

// File protocol
struct EFI_FILE_PROTOCOL {
    UINT64 Revision;
    EFI_STATUS (*Open)(
        struct EFI_FILE_PROTOCOL *This,
        struct EFI_FILE_PROTOCOL **NewHandle,
        CHAR16 *FileName,
        UINT64 OpenMode,
        UINT64 Attributes
    );
    EFI_STATUS (*Close)(struct EFI_FILE_PROTOCOL *This);
    EFI_STATUS (*Delete)(struct EFI_FILE_PROTOCOL *This);
    EFI_STATUS (*Read)(
        struct EFI_FILE_PROTOCOL *This,
        UINTN *BufferSize,
        VOID *Buffer
    );
    // ... (add more if needed)
};

struct EFI_SIMPLE_FILE_SYSTEM_PROTOCOL {
    UINT64 Revision;
    EFI_STATUS (*OpenVolume)(
        struct EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *This,
        struct EFI_FILE_PROTOCOL **Root
    );
};

// Console output protocol with required members
struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL {
    EFI_STATUS (*Reset)(struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This, UINT8 ExtVerify);
    EFI_STATUS (*OutputString)(struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This, const CHAR16 *String);
    EFI_STATUS (*TestString)(struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This, const CHAR16 *String);
    EFI_STATUS (*QueryMode)(struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This, UINTN ModeNumber, UINTN *Columns, UINTN *Rows);
    EFI_STATUS (*SetMode)(struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This, UINTN ModeNumber);
    EFI_STATUS (*SetAttribute)(struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This, UINTN Attribute);
    EFI_STATUS (*ClearScreen)(struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This);
    EFI_STATUS (*SetCursorPosition)(struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This, UINTN Column, UINTN Row);
    EFI_STATUS (*EnableCursor)(struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This, UINT8 Visible);
    // ... (add more if needed)
};

// Text color macros
#define EFI_BLACK          0x00
#define EFI_BLUE           0x01
#define EFI_GREEN          0x02
#define EFI_CYAN           0x03
#define EFI_RED            0x04
#define EFI_MAGENTA        0x05
#define EFI_BROWN          0x06
#define EFI_LIGHTGRAY      0x07
#define EFI_BRIGHT         0x08
#define EFI_DARKGRAY       0x08
#define EFI_LIGHTBLUE      0x09
#define EFI_LIGHTGREEN     0x0A
#define EFI_LIGHTCYAN      0x0B
#define EFI_LIGHTRED       0x0C
#define EFI_LIGHTMAGENTA   0x0D
#define EFI_YELLOW         0x0E
#define EFI_WHITE          0x0F

// EFI_TEXT_ATTR macro
#define EFI_TEXT_ATTR(f, b)   ((f) | ((b) << 4))

#endif // EFI_H
