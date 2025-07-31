#include "../include/efi.h"
#include "../include/bootinfo.h"

// --- Minimal Hex Printer for CHAR16 (prints 0x...64bit) ---
static void uefi_hex16(CHAR16 *buf, uint64_t val) {
    buf[0] = L'0'; buf[1] = L'x';
    int shift = 60;
    for (int i = 0; i < 16; ++i, shift -= 4)
        buf[2 + i] = L"0123456789ABCDEF"[(val >> shift) & 0xF];
    buf[18] = 0;
}

static void print_hex(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *ConOut, const CHAR16 *prefix, uint64_t val) {
    CHAR16 buf[64];
    int idx = 0;
    if (prefix) while (prefix[idx]) { buf[idx] = prefix[idx]; idx++; }
    buf[idx] = 0;
    uefi_hex16(buf + idx, val);
    for (int i = 0; buf[i]; ++i);
    buf[idx + 18] = L'\r'; buf[idx + 19] = L'\n'; buf[idx + 20] = 0;
    ConOut->OutputString(ConOut, buf);
}

// --- ELF structs, minimal ---
typedef struct {
    unsigned char e_ident[16];
    uint16_t e_type, e_machine;
    uint32_t e_version;
    uint64_t e_entry, e_phoff, e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize, e_phentsize, e_phnum, e_shentsize, e_shnum, e_shstrndx;
} __attribute__((packed)) Elf64_Ehdr;

typedef struct {
    uint32_t p_type, p_flags;
    uint64_t p_offset, p_vaddr, p_paddr, p_filesz, p_memsz, p_align;
} __attribute__((packed)) Elf64_Phdr;

#define PT_LOAD 1

// --- Minimal memcmp, memset (standalone) ---
static int memcmp(const void *a, const void *b, unsigned n) {
    const unsigned char *p = a, *q = b;
    for (unsigned i = 0; i < n; ++i) if (p[i] != q[i]) return p[i] - q[i];
    return 0;
}
static void *memset(void *d, int v, unsigned n) {
    unsigned char *p = d; for (unsigned i = 0; i < n; ++i) p[i] = v; return d;
}

// Standalone loader. Pass already-opened kernel file, and output pointer to entry.
EFI_STATUS load_and_boot_kernel(
    EFI_FILE_PROTOCOL *KernelFile,
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *ConOut,
    bootinfo_t *bootinfo)
{
    EFI_STATUS st;
    UINTN sz;

    // --- 1. Read ELF header ---
    Elf64_Ehdr eh;
    sz = sizeof(eh);
    st = KernelFile->Read(KernelFile, &sz, &eh);
    if (st || sz != sizeof(eh)) { ConOut->OutputString(ConOut, L"ELF header read error\r\n"); return 1; }
    if (memcmp(eh.e_ident, "\x7f""ELF", 4) != 0 || eh.e_ident[4] != 2) {
        ConOut->OutputString(ConOut, L"Not ELF64\r\n"); return 2;
    }

    // --- 2. Load segments ---
    for (UINTN i = 0; i < eh.e_phnum; ++i) {
        Elf64_Phdr ph;
        // Seek to program header
        KernelFile->SetPosition(KernelFile, eh.e_phoff + i * sizeof(ph));
        sz = sizeof(ph);
        st = KernelFile->Read(KernelFile, &sz, &ph);
        if (st || sz != sizeof(ph)) { ConOut->OutputString(ConOut, L"PH read fail\r\n"); return 3; }
        if (ph.p_type != PT_LOAD || ph.p_memsz == 0) continue;

        // Print info
        print_hex(ConOut, L"Seg: i=", i);
        print_hex(ConOut, L"   addr=", ph.p_paddr);
        print_hex(ConOut, L"   filesz=", ph.p_filesz);
        print_hex(ConOut, L"   memsz=", ph.p_memsz);

        EFI_PHYSICAL_ADDRESS dest = ph.p_paddr;
        UINTN npages = (ph.p_memsz + 4095) / 4096;
        st = gBS->AllocatePages(EFI_ALLOCATE_ADDRESS, EfiLoaderData, npages, &dest);
        if (st || dest != ph.p_paddr) {
            ConOut->OutputString(ConOut, L"Page alloc failed\r\n");
            return 4;
        }
        KernelFile->SetPosition(KernelFile, ph.p_offset);
        sz = ph.p_filesz;
        st = KernelFile->Read(KernelFile, &sz, (void *)(UINTN)dest);
        if (st || sz != ph.p_filesz) {
            ConOut->OutputString(ConOut, L"Segment read fail\r\n");
            return 5;
        }
        // Zero rest of segment
        if (ph.p_memsz > ph.p_filesz)
            memset((void *)(UINTN)(dest + ph.p_filesz), 0, ph.p_memsz - ph.p_filesz);
    }

    // --- 3. Call kernel entry
    void (*entry)(bootinfo_t*) = (void (*)(bootinfo_t*))(UINTN)eh.e_entry;
    bootinfo->kernel_entry = (void *)(UINTN)eh.e_entry;
    print_hex(ConOut, L"Kernel entry: ", eh.e_entry);
    entry(bootinfo);
    return 0;
}
