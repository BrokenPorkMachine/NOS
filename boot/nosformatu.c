#include "efi.h"
#include <stdint.h>
#include <stddef.h>

void __chkstk(void) {}

static void print_ascii(EFI_SYSTEM_TABLE *st, const char *s) {
    CHAR16 buf[128];
    size_t i = 0;
    while (s[i] && i < 127) {
        buf[i] = (CHAR16)s[i];
        i++;
    }
    buf[i] = 0;
    st->ConOut->OutputString(st->ConOut, buf);
}

static void print_dec(EFI_SYSTEM_TABLE *st, uint64_t v) {
    char buf[32];
    char *p = buf + sizeof(buf) - 1;
    *p = 0;
    if (v == 0) *--p = '0';
    while (v) {
        *--p = '0' + (v % 10);
        v /= 10;
    }
    print_ascii(st, p);
}

static char read_char(EFI_SYSTEM_TABLE *st) {
    EFI_INPUT_KEY key;
    EFI_STATUS status;
    for (;;) {
        status = st->ConIn->ReadKeyStroke(st->ConIn, &key);
        if (!EFI_ERROR(status)) {
            if (key.UnicodeChar) {
                CHAR16 out[2] = { key.UnicodeChar, 0 };
                st->ConOut->OutputString(st->ConOut, out);
                return (char)key.UnicodeChar;
            }
        }
    }
}

// MBR partition entry
typedef struct {
    UINT8  boot_indicator;
    UINT8  start_head;
    UINT8  start_sector;
    UINT8  start_cylinder;
    UINT8  partition_type;
    UINT8  end_head;
    UINT8  end_sector;
    UINT8  end_cylinder;
    UINT32 start_lba;
    UINT32 sectors;
} __attribute__((packed)) mbr_part_t;

typedef EFI_STATUS (EFIAPI *EFI_LOCATE_HANDLE_BUFFER)(UINTN, EFI_GUID*, VOID*, UINTN*, EFI_HANDLE**);

EFI_STATUS EFIAPI efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    print_ascii(SystemTable, "NOSFormatU - NitrOS Disk Format Utility\r\n");

    EFI_LOCATE_HANDLE_BUFFER LocateHandleBuffer = (EFI_LOCATE_HANDLE_BUFFER)SystemTable->BootServices->LocateHandleBuffer;
    EFI_HANDLE *handles = NULL;
    UINTN handle_count = 0;
    EFI_STATUS status = LocateHandleBuffer(EFI_LOCATE_SEARCH_BY_PROTOCOL, (EFI_GUID*)&gEfiBlockIoProtocolGuid, NULL, &handle_count, &handles);
    if (EFI_ERROR(status) || handle_count == 0) {
        print_ascii(SystemTable, "No block devices found\r\n");
        return status;
    }

    for (UINTN i = 0; i < handle_count; ++i) {
        EFI_BLOCK_IO_PROTOCOL *bio;
        if (EFI_ERROR(SystemTable->BootServices->HandleProtocol(handles[i], (EFI_GUID*)&gEfiBlockIoProtocolGuid, (void**)&bio)))
            continue;
        uint64_t size = (bio->Media->LastBlock + 1) * bio->Media->BlockSize;
        print_ascii(SystemTable, "Disk ");
        print_dec(SystemTable, i);
        print_ascii(SystemTable, ": ");
        print_dec(SystemTable, size / 1024);
        print_ascii(SystemTable, " KB\r\n");
    }

    print_ascii(SystemTable, "Select disk number to format or 'q' to quit: ");
    char c = read_char(SystemTable);
    print_ascii(SystemTable, "\r\n");
    if (c == 'q' || c == 'Q')
        return EFI_SUCCESS;
    UINTN index = (UINTN)(c - '0');
    if (index >= handle_count) {
        print_ascii(SystemTable, "Invalid selection\r\n");
        return EFI_SUCCESS;
    }

    EFI_BLOCK_IO_PROTOCOL *bio;
    status = SystemTable->BootServices->HandleProtocol(handles[index], (EFI_GUID*)&gEfiBlockIoProtocolGuid, (void**)&bio);
    if (EFI_ERROR(status))
        return status;

    UINT8 sector[512];
    for (UINTN i = 0; i < 512; ++i) sector[i] = 0;
    mbr_part_t *part = (mbr_part_t*)(sector + 446);
    part[0].boot_indicator = 0;
    part[0].partition_type = 0x07;
    part[0].start_lba = 1;
    part[0].sectors = (UINT32)(bio->Media->LastBlock);
    sector[510] = 0x55;
    sector[511] = 0xAA;

    status = bio->WriteBlocks(bio, bio->Media->MediaId, 0, 512, sector);
    if (EFI_ERROR(status))
        print_ascii(SystemTable, "Write failed\r\n");
    else
        print_ascii(SystemTable, "Disk formatted with single partition\r\n");

    return EFI_SUCCESS;
}
