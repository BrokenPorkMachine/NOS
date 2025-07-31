#ifndef EFI_H
#define EFI_H

#define EFIAPI __attribute__((ms_abi))

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
#define EFI_SUCCESS 0
#define EFI_LOAD_ERROR (EFI_STATUS)(1ULL | (1ULL << 63))

// File Modes
#define EFI_FILE_MODE_READ 0x0000000000000001ULL

// Memory allocation types
#define EFI_ALLOCATE_ANY_PAGES 0
#define EFI_ALLOCATE_ADDRESS   2

// Memory types
#define EfiLoaderData 2

// Text Attributes
#define EFI_BLACK 0x0
#define EFI_YELLOW 0xE
#define EFI_LIGHTGRAY 0x7
#define EFI_WHITE 0xF
#define EFI_BLUE 0x1
#define EFI_LIGHTGREEN 0xA
#define EFI_LIGHTRED 0xC
#define EFI_TEXT_ATTR(fg, bg) ((fg) | ((bg) << 4))

// EFI SIMPLE TEXT OUTPUT PROTOCOL
typedef struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL {
    EFI_STATUS (EFIAPI *Reset)(struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL*, BOOLEAN);
    EFI_STATUS (EFIAPI *OutputString)(struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL*, CHAR16*);
    EFI_STATUS (EFIAPI *TestString)(struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL*, CHAR16*);
    EFI_STATUS (EFIAPI *QueryMode)(struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL*, UINTN, UINTN*, UINTN*);
    EFI_STATUS (EFIAPI *SetMode)(struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL*, UINTN);
    EFI_STATUS (EFIAPI *SetAttribute)(struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL*, UINTN);
    EFI_STATUS (EFIAPI *ClearScreen)(struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL*);
    EFI_STATUS (EFIAPI *SetCursorPosition)(struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL*, UINTN, UINTN);
    EFI_STATUS (EFIAPI *EnableCursor)(struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL*, BOOLEAN);
} EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL;

// EFI FILE PROTOCOL
typedef struct EFI_FILE_PROTOCOL {
    UINT64 Revision;
    EFI_STATUS (EFIAPI *Open)(struct EFI_FILE_PROTOCOL*, struct EFI_FILE_PROTOCOL**, CHAR16*, UINT64, UINT64);
    EFI_STATUS (EFIAPI *Close)(struct EFI_FILE_PROTOCOL*);
    EFI_STATUS (EFIAPI *Delete)(struct EFI_FILE_PROTOCOL*);
    EFI_STATUS (EFIAPI *Read)(struct EFI_FILE_PROTOCOL*, UINTN*, VOID*);
    EFI_STATUS (EFIAPI *Write)(struct EFI_FILE_PROTOCOL*, UINTN*, VOID*);
    EFI_STATUS (EFIAPI *GetPosition)(struct EFI_FILE_PROTOCOL*, UINT64*);
    EFI_STATUS (EFIAPI *SetPosition)(struct EFI_FILE_PROTOCOL*, UINT64);
    EFI_STATUS (EFIAPI *GetInfo)(struct EFI_FILE_PROTOCOL*, EFI_GUID*, UINTN*, VOID*);
    EFI_STATUS (EFIAPI *SetInfo)(struct EFI_FILE_PROTOCOL*, EFI_GUID*, UINTN, VOID*);
    EFI_STATUS (EFIAPI *Flush)(struct EFI_FILE_PROTOCOL*);
} EFI_FILE_PROTOCOL;

// EFI SIMPLE FILE SYSTEM PROTOCOL
typedef struct EFI_SIMPLE_FILE_SYSTEM_PROTOCOL {
    UINT64 Revision;
    EFI_STATUS (EFIAPI *OpenVolume)(struct EFI_SIMPLE_FILE_SYSTEM_PROTOCOL*, EFI_FILE_PROTOCOL**);
} EFI_SIMPLE_FILE_SYSTEM_PROTOCOL;

// EFI BOOT SERVICES
typedef struct EFI_BOOT_SERVICES {
    char _pad1[24+8];
    EFI_STATUS (EFIAPI *AllocatePages)(UINTN, UINTN, UINTN, EFI_PHYSICAL_ADDRESS*);
    EFI_STATUS (EFIAPI *FreePages)(EFI_PHYSICAL_ADDRESS, UINTN);
    EFI_STATUS (EFIAPI *GetMemoryMap)(UINTN*, VOID*, UINTN*, UINTN*, UINT32*);
    EFI_STATUS (EFIAPI *AllocatePool)(UINTN, UINTN, VOID**);
    EFI_STATUS (EFIAPI *FreePool)(VOID*);
    char _pad2[24];
    EFI_STATUS (EFIAPI *HandleProtocol)(EFI_HANDLE, EFI_GUID*, VOID**);
    char _pad3[64+40+16];
    EFI_STATUS (EFIAPI *ExitBootServices)(EFI_HANDLE, UINTN);
} EFI_BOOT_SERVICES;

// EFI SYSTEM TABLE
typedef struct {
    char _pad[44];
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *ConOut;
    VOID *ConIn;
    EFI_BOOT_SERVICES *BootServices;
} EFI_SYSTEM_TABLE;

// GUIDs
static const EFI_GUID gEfiSimpleFileSystemProtocolGuid = {
    0x964e5b21, 0x6459, 0x11d2,
    {0x8e,0x39,0x00,0xa0,0xc9,0x69,0x72,0x3b}
};

#endif // EFI_H
