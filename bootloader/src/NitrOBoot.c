#include "../include/efi.h"

#define KERNEL_PATH L"\\EFI\\BOOT\\kernel.bin"
#define KERNEL_MAX_SIZE (2 * 1024 * 1024)

typedef struct {
    unsigned char e_ident[16];
    UINT16 e_type;
    UINT16 e_machine;
    UINT32 e_version;
    UINT64 e_entry;
    UINT64 e_phoff;
    UINT64 e_shoff;
    UINT32 e_flags;
    UINT16 e_ehsize;
    UINT16 e_phentsize;
    UINT16 e_phnum;
    UINT16 e_shentsize;
    UINT16 e_shnum;
    UINT16 e_shstrndx;
} __attribute__((packed)) Elf64_Ehdr;

typedef struct {
    UINT32 p_type;
    UINT32 p_flags;
    UINT64 p_offset;
    UINT64 p_vaddr;
    UINT64 p_paddr;
    UINT64 p_filesz;
    UINT64 p_memsz;
    UINT64 p_align;
} __attribute__((packed)) Elf64_Phdr;

static void print_step(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *ConOut, CHAR16 *msg) {
    ConOut->SetAttribute(ConOut, EFI_TEXT_ATTR(EFI_YELLOW, EFI_BLACK));
    ConOut->OutputString(ConOut, msg);
    ConOut->SetAttribute(ConOut, EFI_TEXT_ATTR(EFI_LIGHTGRAY, EFI_BLACK));
}

static void print_ok(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *ConOut) {
    ConOut->SetAttribute(ConOut, EFI_TEXT_ATTR(EFI_LIGHTGREEN, EFI_BLACK));
    ConOut->OutputString(ConOut, L" [OK]\r\n");
    ConOut->SetAttribute(ConOut, EFI_TEXT_ATTR(EFI_LIGHTGRAY, EFI_BLACK));
}

static void print_fail(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *ConOut, CHAR16 *msg) {
    ConOut->SetAttribute(ConOut, EFI_TEXT_ATTR(EFI_LIGHTRED, EFI_BLACK));
    ConOut->OutputString(ConOut, L" [FAIL]\r\n");
    if (msg) ConOut->OutputString(ConOut, msg);
    ConOut->SetAttribute(ConOut, EFI_TEXT_ATTR(EFI_LIGHTGRAY, EFI_BLACK));
}

EFI_STATUS efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    EFI_STATUS status;
    EFI_BOOT_SERVICES *BS = SystemTable->BootServices;
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *ConOut = SystemTable->ConOut;

    ConOut->SetAttribute(ConOut, EFI_TEXT_ATTR(EFI_WHITE, EFI_BLUE));
    ConOut->ClearScreen(ConOut);
    ConOut->OutputString(ConOut, L"NitrOBoot Loader Starting...\r\n");
    ConOut->SetAttribute(ConOut, EFI_TEXT_ATTR(EFI_LIGHTGRAY, EFI_BLACK));

    // (1) Locate Simple FileSystem
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *FileSystem;
    print_step(ConOut, L"[1] Locate FileSystem protocol...");
    status = BS->HandleProtocol(ImageHandle, (EFI_GUID*)&gEfiSimpleFileSystemProtocolGuid, (VOID**)&FileSystem);
    if (status != EFI_SUCCESS) {
        print_fail(ConOut, L"Cannot locate FileSystem protocol\r\n");
        return status;
    }
    print_ok(ConOut);

    // (2) Open root
    EFI_FILE_PROTOCOL *Root;
    print_step(ConOut, L"[2] Open root volume...");
    status = FileSystem->OpenVolume(FileSystem, &Root);
    if (status != EFI_SUCCESS) {
        print_fail(ConOut, L"Cannot open root volume\r\n");
        return status;
    }
    print_ok(ConOut);

    // (3) Open kernel file
    EFI_FILE_PROTOCOL *KernelFile;
    print_step(ConOut, L"[3] Open kernel file...");
    status = Root->Open(Root, &KernelFile, KERNEL_PATH, EFI_FILE_MODE_READ, 0);
    if (status != EFI_SUCCESS) {
        print_fail(ConOut, L"Cannot open kernel.bin\r\n");
        return status;
    }
    print_ok(ConOut);

    // (4) Allocate buffer and read ELF
    EFI_PHYSICAL_ADDRESS kernel_load_addr = 0x100000;
    UINTN kernel_pages = (KERNEL_MAX_SIZE + 4095) / 4096;
    print_step(ConOut, L"[4] Allocate pages for kernel...");
    status = BS->AllocatePages(EFI_ALLOCATE_ADDRESS, EfiLoaderData, kernel_pages, &kernel_load_addr);
    if (status != EFI_SUCCESS) {
        print_fail(ConOut, L"Cannot allocate kernel buffer\r\n");
        return status;
    }
    print_ok(ConOut);

    UINTN kernel_size = KERNEL_MAX_SIZE;
    print_step(ConOut, L"[5] Read kernel into buffer...");
    status = KernelFile->Read(KernelFile, &kernel_size, (VOID*)kernel_load_addr);
    KernelFile->Close(KernelFile);
    if (status != EFI_SUCCESS || kernel_size == 0) {
        print_fail(ConOut, L"Failed to read kernel\r\n");
        return EFI_LOAD_ERROR;
    }
    print_ok(ConOut);

    // (5) Parse ELF headers
    print_step(ConOut, L"[6] Parse ELF headers...");
    Elf64_Ehdr *ehdr = (Elf64_Ehdr *)(UINTN)kernel_load_addr;
    Elf64_Phdr *phdrs = (Elf64_Phdr *)(UINTN)(kernel_load_addr + ehdr->e_phoff);

    // (6) Load all PT_LOAD segments
    for (UINT16 i = 0; i < ehdr->e_phnum; i++) {
        if (phdrs[i].p_type != 1) continue; // PT_LOAD

        UINT64 dest = phdrs[i].p_paddr;
        UINT64 src  = kernel_load_addr + phdrs[i].p_offset;

        UINTN pages = (phdrs[i].p_memsz + 4095) / 4096;
        BS->AllocatePages(EFI_ALLOCATE_ADDRESS, EfiLoaderData, pages, &dest);

        BS->CopyMem((VOID *)(UINTN)dest, (VOID *)(UINTN)src, phdrs[i].p_filesz);

        if (phdrs[i].p_memsz > phdrs[i].p_filesz) {
            UINTN bss_len = phdrs[i].p_memsz - phdrs[i].p_filesz;
            BS->SetMem((VOID *)(UINTN)(dest + phdrs[i].p_filesz), bss_len, 0);
        }
    }
    print_ok(ConOut);

    // (7) Exit Boot Services
    UINTN mmap_size = 0, map_key, desc_size;
    UINT32 desc_ver;
    print_step(ConOut, L"[7] Get memory map...");
    BS->GetMemoryMap(&mmap_size, NULL, &map_key, &desc_size, &desc_ver);
    mmap_size += desc_size * 8;

    EFI_PHYSICAL_ADDRESS mmap_addr;
    status = BS->AllocatePages(EFI_ALLOCATE_ANY_PAGES, EfiLoaderData, (mmap_size + 4095) / 4096, &mmap_addr);
    EFI_MEMORY_DESCRIPTOR *memory_map = (EFI_MEMORY_DESCRIPTOR*)(UINTN)mmap_addr;
    status = BS->GetMemoryMap(&mmap_size, memory_map, &map_key, &desc_size, &desc_ver);
    if (status != EFI_SUCCESS) {
        print_fail(ConOut, L"GetMemoryMap failed\r\n");
        return status;
    }

    status = BS->ExitBootServices(ImageHandle, map_key);
    if (status != EFI_SUCCESS) {
        print_fail(ConOut, L"ExitBootServices failed\r\n");
        return status;
    }
    print_ok(ConOut);

    // (8) Jump to kernel
    print_step(ConOut, L"[8] Jumping to kernel...\r\n");
    void (*entry)(void *) = (void (*)(void *))(UINTN)ehdr->e_entry;
    entry(NULL);

    for (;;);
    return EFI_SUCCESS;
}
