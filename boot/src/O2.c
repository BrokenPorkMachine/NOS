#include "efi.h"
#include "bootinfo.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#define KERNEL_NAME L"\\kernel.bin"
#define MH_MAGIC_64    0xFEEDFACF
#define FAT_MAGIC      0xCAFEBABE
#define MAX_KERNEL_SEGMENTS 16
/*
// --- Kernel segment info ---
typedef struct {
    uint64_t vaddr, paddr, filesz, memsz;
    uint32_t flags;
    char name[17];
} kernel_segment_t;
*/
// --- Kernel type ---
typedef enum {
    KERNEL_TYPE_UNKNOWN=0, KERNEL_TYPE_ELF64,
    KERNEL_TYPE_MACHO64, KERNEL_TYPE_MACHO_FAT,
    KERNEL_TYPE_PECOFF, KERNEL_TYPE_FLAT
} kernel_type_t;

// --- Loader function prototypes ---
kernel_type_t detect_kernel_type(const uint8_t *data, size_t size);

EFI_STATUS load_file(EFI_SYSTEM_TABLE *st, EFI_FILE_PROTOCOL *root,
                     const CHAR16 *path, void **buf, UINTN *size);

EFI_STATUS load_elf64(EFI_SYSTEM_TABLE *st, const void *image, UINTN size, void **entry,
                      kernel_segment_t *segs, uint32_t *segc);

EFI_STATUS load_macho64(EFI_SYSTEM_TABLE *st, const void *image, UINTN size, void **entry,
                        kernel_segment_t *segs, uint32_t *segc);

EFI_STATUS load_macho_fat(EFI_SYSTEM_TABLE *st, const void *image, UINTN size, void **entry,
                          kernel_segment_t *segs, uint32_t *segc);

EFI_STATUS load_pe_coff(EFI_SYSTEM_TABLE *st, const void *image, UINTN size, void **entry,
                        kernel_segment_t *segs, uint32_t *segc);

EFI_STATUS load_flat(EFI_SYSTEM_TABLE *st, const void *image, UINTN size, void **entry,
                     kernel_segment_t *segs, uint32_t *segc);

// --- Minimal C stdlib ---
static void *memcpy(void *dst, const void *src, size_t n) { uint8_t *d=dst; const uint8_t *s=src; while (n--) *d++ = *s++; return dst; }
static void *memset(void *dst, int c, size_t n) { uint8_t *d=dst; while (n--) *d++ = (uint8_t)c; return dst; }
static int memcmp(const void *a, const void *b, size_t n) { const uint8_t *x=a, *y=b; while (n--) { if (*x!=*y) return *x-*y; x++; y++; } return 0; }
static size_t strlen(const char *s) { size_t i=0; while(s[i]) ++i; return i; }
static void strcpy(char *dst, const char *src) { while((*dst++ = *src++)); }

static uint64_t g_kernel_base = 0, g_kernel_size = 0;

// --- UEFI output helpers ---
static void print_ascii(EFI_SYSTEM_TABLE *st, const char *s) {
    CHAR16 buf[256]; size_t i=0;
    while (i < 255 && s[i]) { buf[i] = (CHAR16)s[i]; i++; }
    buf[i]=0;
    st->ConOut->OutputString(st->ConOut, buf);
}
static void print_hex(EFI_SYSTEM_TABLE *st, uint64_t val) {
    char buf[19] = "0x0000000000000000";
    for (int i = 0; i < 16; ++i)
        buf[17 - i] = "0123456789ABCDEF"[(val >> (i * 4)) & 0xF];
    print_ascii(st, buf);
}
static void print_dec(EFI_SYSTEM_TABLE *st, uint64_t v) {
    char buf[22], *p = buf+21; *p = 0;
    if (!v) *--p = '0';
    while (v) { *--p = '0'+(v%10); v/=10; }
    print_ascii(st, p);
}

// --- Segment debug output ---
static void log_bootinfo(EFI_SYSTEM_TABLE *st, const bootinfo_t *bi) {
    print_ascii(st, "[bootinfo] magic: "); print_hex(st, bi->magic); print_ascii(st, "\r\n");
    print_ascii(st, "[bootinfo] kernel: base="); print_hex(st, bi->kernel_load_base);
    print_ascii(st, " size="); print_hex(st, bi->kernel_load_size); print_ascii(st, "\r\n");
    print_ascii(st, "[bootinfo] kernel_entry: "); print_hex(st, (uint64_t)bi->kernel_entry); print_ascii(st, "\r\n");
    print_ascii(st, "[bootinfo] kernel_segment_count: "); print_dec(st, bi->kernel_segment_count); print_ascii(st, "\r\n");
    for (uint32_t i=0; i < bi->kernel_segment_count; i++) {
        print_ascii(st, "  [seg] vaddr="); print_hex(st, bi->kernel_segments[i].vaddr);
        print_ascii(st, " paddr="); print_hex(st, bi->kernel_segments[i].paddr);
        print_ascii(st, " filesz="); print_hex(st, bi->kernel_segments[i].filesz);
        print_ascii(st, " memsz="); print_hex(st, bi->kernel_segments[i].memsz);
        print_ascii(st, " flags="); print_hex(st, bi->kernel_segments[i].flags);
        if (bi->kernel_segments[i].name[0]) {
            print_ascii(st, " name=");
            print_ascii(st, bi->kernel_segments[i].name);
        }
        print_ascii(st, "\r\n");
    }
}

// --- Kernel type detection ---
static kernel_type_t detect_kernel_type(const uint8_t *data, size_t size) {
    if (size >= 4 && data[0]==0x7F && data[1]=='E' && data[2]=='L' && data[3]=='F' && data[4]==2)
        return KERNEL_TYPE_ELF64;
    uint32_t magic = *(const uint32_t*)data;
    if (magic == MH_MAGIC_64) return KERNEL_TYPE_MACHO64;
    if (magic == FAT_MAGIC) return KERNEL_TYPE_MACHO_FAT;
    if (size >= 0x40) {
        if (data[0] == 'M' && data[1] == 'Z') {
            int32_t lfanew = *(int32_t*)&data[0x3C];
            if ((size_t)lfanew + 4 < size && data[lfanew] == 'P' && data[lfanew+1] == 'E')
                return KERNEL_TYPE_PECOFF;
        }
    }
    return KERNEL_TYPE_FLAT;
}

// --- Load file from FS ---
EFI_STATUS load_file(EFI_SYSTEM_TABLE *st, EFI_FILE_PROTOCOL *root,
                     const CHAR16 *path, void **buf, UINTN *size) {
    EFI_STATUS status;
    EFI_FILE_PROTOCOL *file;
    status = root->Open(root, &file, (CHAR16 *)path, 0x00000001, 0);
    if (EFI_ERROR(status)) return status;
    UINTN info_size = 0;
    status = file->GetInfo(file, (EFI_GUID *)&gEfiFileInfoGuid, &info_size, NULL);
    if (status != EFI_BUFFER_TOO_SMALL) { file->Close(file); return status; }
    EFI_FILE_INFO *info;
    status = st->BootServices->AllocatePool(EfiLoaderData, info_size, (void **)&info);
    if (EFI_ERROR(status)) { file->Close(file); return status; }
    status = file->GetInfo(file, (EFI_GUID *)&gEfiFileInfoGuid, &info_size, info);
    if (EFI_ERROR(status)) { st->BootServices->FreePool(info); file->Close(file); return status; }
    *size = info->FileSize;
    st->BootServices->FreePool(info);
    status = st->BootServices->AllocatePool(EfiLoaderData, *size, buf);
    if (EFI_ERROR(status)) { file->Close(file); return status; }
    UINTN to_read = *size;
    status = file->Read(file, &to_read, *buf);
    file->Close(file);
    return status;
}

// --- FLAT loader ---
EFI_STATUS load_flat(EFI_SYSTEM_TABLE *st, const void *image, UINTN size, void **entry,
                     kernel_segment_t *segs, uint32_t *segc) {
    EFI_PHYSICAL_ADDRESS addr = 0x100000;
    UINTN pages = (size + 0xFFF) / 0x1000;
    EFI_STATUS s = st->BootServices->AllocatePages(EFI_ALLOCATE_ADDRESS, EfiLoaderData, pages, &addr);
    if (EFI_ERROR(s)) return s;
    memcpy((void *)(uintptr_t)addr, image, size);
    print_ascii(st, "[O2] FLAT LOAD: paddr="); print_hex(st, addr); print_ascii(st, " size="); print_hex(st, size); print_ascii(st, "\r\n");
    if (segs && segc) {
        segs[0].vaddr = addr;
        segs[0].paddr = addr;
        segs[0].filesz = size;
        segs[0].memsz = size;
        segs[0].flags = 0x7;
        strcpy(segs[0].name, "flat");
        *segc = 1;
    }
    g_kernel_base = addr;
    g_kernel_size = size;
    *entry = (void *)(uintptr_t)addr;
    return EFI_SUCCESS;
}

// --- ELF64 loader ---
EFI_STATUS load_elf64(EFI_SYSTEM_TABLE *st, const void *image, UINTN size, void **entry,
                      kernel_segment_t *segs, uint32_t *segc) {
    typedef struct { unsigned char e_ident[16]; UINT16 e_type,e_machine; UINT32 e_version; UINT64 e_entry,e_phoff,e_shoff; UINT32 e_flags; UINT16 e_ehsize,e_phentsize,e_phnum; UINT16 e_shentsize,e_shnum,e_shstrndx; } Elf64_Ehdr;
    typedef struct { UINT32 p_type,p_flags; UINT64 p_offset,p_vaddr,p_paddr,p_filesz,p_memsz,p_align; } Elf64_Phdr;
    if (size < sizeof(Elf64_Ehdr)) return EFI_LOAD_ERROR;
    const Elf64_Ehdr *eh = (const Elf64_Ehdr *)image;
    if (!(eh->e_ident[0]==0x7F && eh->e_ident[1]=='E' && eh->e_ident[2]=='L' && eh->e_ident[3]=='F')) return EFI_LOAD_ERROR;
    const Elf64_Phdr *ph = (const Elf64_Phdr *)((const UINT8 *)image + eh->e_phoff);
    UINT64 first = (UINT64)-1, last = 0;
    uint32_t count = 0;
    for (UINT16 i = 0; i < eh->e_phnum; ++i, ++ph) {
        if (ph->p_type != 1) continue;
        UINTN pages = (ph->p_memsz + 0xFFF) / 0x1000;
        if (!pages) continue;
        EFI_PHYSICAL_ADDRESS seg = ph->p_paddr;
        if (!seg)
            seg = ph->p_vaddr;
        EFI_STATUS s = st->BootServices->AllocatePages(EFI_ALLOCATE_ADDRESS, EfiLoaderData, pages, &seg);
        if (EFI_ERROR(s)) return s;
        memcpy((void *)(uintptr_t)seg, (const UINT8 *)image + ph->p_offset, ph->p_filesz);
        if (ph->p_memsz > ph->p_filesz)
            memset((void *)(uintptr_t)(seg + ph->p_filesz), 0, ph->p_memsz - ph->p_filesz);

        print_ascii(st, "[O2] ELF LOAD: vaddr="); print_hex(st, ph->p_vaddr);
        print_ascii(st, " paddr="); print_hex(st, seg);
        print_ascii(st, " filesz="); print_hex(st, ph->p_filesz);
        print_ascii(st, " memsz="); print_hex(st, ph->p_memsz);
        print_ascii(st, " flags="); print_hex(st, ph->p_flags); print_ascii(st, "\r\n");

        if (segs && segc && count < MAX_KERNEL_SEGMENTS) {
            segs[count].vaddr  = ph->p_vaddr;
            segs[count].paddr  = seg;
            segs[count].filesz = ph->p_filesz;
            segs[count].memsz  = ph->p_memsz;
            segs[count].flags  = ph->p_flags;
            segs[count].name[0]=0;
            ++count;
        }
        if (seg < first) first = seg;
        if (seg + ph->p_memsz > last) last = seg + ph->p_memsz;
    }
    if (segc) *segc = count;
    g_kernel_base = first;
    g_kernel_size = last - first;
    *entry = (void *)(uintptr_t)eh->e_entry;
    return EFI_SUCCESS;
}

// --- PE/COFF loader ---
#define IMAGE_DOS_SIGNATURE 0x5A4D
#define IMAGE_NT_SIGNATURE  0x00004550
typedef struct { uint16_t e_magic; uint16_t e_cblp,e_cp,e_crlc,e_cparhdr,e_minalloc,e_maxalloc,e_ss,e_sp,e_csum,e_ip,e_cs,e_lfarlc,e_ovno,e_res[4],e_oemid,e_oeminfo,e_res2[10]; int32_t e_lfanew; } IMAGE_DOS_HEADER;
typedef struct { uint32_t Signature; uint16_t Machine,NumberOfSections; uint32_t TimeDateStamp,PointerToSymbolTable,NumberOfSymbols; uint16_t SizeOfOptionalHeader,Characteristics; } IMAGE_NT_HEADERS64;
typedef struct { uint16_t Magic; uint8_t  MajorLinkerVersion,MinorLinkerVersion; uint32_t SizeOfCode,SizeOfInitializedData,SizeOfUninitializedData,AddressOfEntryPoint,BaseOfCode; uint64_t ImageBase; uint32_t SectionAlignment,FileAlignment; uint16_t MajorOperatingSystemVersion,MinorOperatingSystemVersion,MajorImageVersion,MinorImageVersion,MajorSubsystemVersion,MinorSubsystemVersion; uint32_t Win32VersionValue,SizeOfImage,SizeOfHeaders,CheckSum; uint16_t Subsystem,DllCharacteristics; uint64_t SizeOfStackReserve,SizeOfStackCommit,SizeOfHeapReserve,SizeOfHeapCommit; uint32_t LoaderFlags,NumberOfRvaAndSizes; } IMAGE_OPTIONAL_HEADER64;
typedef struct { uint8_t  Name[8]; union { uint32_t PhysicalAddress,VirtualSize; }; uint32_t VirtualAddress,SizeOfRawData,PointerToRawData,PointerToRelocations,PointerToLinenumbers; uint16_t NumberOfRelocations,NumberOfLinenumbers; uint32_t Characteristics; } IMAGE_SECTION_HEADER;
EFI_STATUS load_pe_coff(EFI_SYSTEM_TABLE *st, const void *image, UINTN size, void **entry,
                        kernel_segment_t *segs, uint32_t *segc) {
    const uint8_t *buf = (const uint8_t*)image;
    const IMAGE_DOS_HEADER *dos = (const IMAGE_DOS_HEADER *)buf;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return EFI_LOAD_ERROR;
    const IMAGE_NT_HEADERS64 *nt = (const IMAGE_NT_HEADERS64 *)(buf + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return EFI_LOAD_ERROR;
    const IMAGE_OPTIONAL_HEADER64 *opt = (const IMAGE_OPTIONAL_HEADER64 *)((const uint8_t*)&nt->SizeOfOptionalHeader + sizeof(uint16_t));
    const IMAGE_SECTION_HEADER *sh = (const IMAGE_SECTION_HEADER *)((const uint8_t*)opt + nt->SizeOfOptionalHeader);
    uint32_t count = 0;
    for (int i = 0; i < nt->NumberOfSections; ++i, ++sh) {
        EFI_PHYSICAL_ADDRESS vaddr = opt->ImageBase + sh->VirtualAddress;
        UINTN pages = (sh->VirtualSize + 0xFFF) / 0x1000;
        EFI_STATUS s = st->BootServices->AllocatePages(EFI_ALLOCATE_ADDRESS, EfiLoaderData, pages, &vaddr);
        if (EFI_ERROR(s)) return s;
        UINTN to_copy = sh->SizeOfRawData;
        if (to_copy > sh->VirtualSize) to_copy = sh->VirtualSize;
        if (to_copy > 0)
            memcpy((void *)(uintptr_t)vaddr, buf + sh->PointerToRawData, to_copy);
        if (sh->VirtualSize > to_copy)
            memset((void *)(uintptr_t)(vaddr + to_copy), 0, sh->VirtualSize - to_copy);

        print_ascii(st, "[O2] PE LOAD: vaddr="); print_hex(st, vaddr); print_ascii(st, " size="); print_hex(st, sh->VirtualSize);
        print_ascii(st, " flags="); print_hex(st, sh->Characteristics); print_ascii(st, " name=");
        for (int n=0; n<8 && sh->Name[n]; n++) { char c[2] = {sh->Name[n], 0}; print_ascii(st, c); }
        print_ascii(st, "\r\n");

        if (segs && segc && count < MAX_KERNEL_SEGMENTS) {
            segs[count].vaddr  = vaddr;
            segs[count].paddr  = vaddr;
            segs[count].filesz = sh->SizeOfRawData;
            segs[count].memsz  = sh->VirtualSize;
            segs[count].flags  = sh->Characteristics;
            for (int n=0; n<8; n++) segs[count].name[n]=sh->Name[n];
            segs[count].name[8]=0;
            ++count;
        }
    }
    if (segc) *segc = count;
    g_kernel_base = opt->ImageBase;
    g_kernel_size = opt->SizeOfImage;
    *entry = (void *)(uintptr_t)(opt->ImageBase + opt->AddressOfEntryPoint);
    return EFI_SUCCESS;
}

// --- Mach-O thin ---
typedef struct { uint32_t magic,cputype,cpusubtype,filetype,ncmds,sizeofcmds,flags,reserved; } macho64_hdr_t;
typedef struct { uint32_t cmd, cmdsize; } macho_loadcmd_t;
typedef struct { uint32_t cmd,cmdsize; char segname[16]; uint64_t vmaddr,vmsize,fileoff,filesize,maxprot,initprot,nsects,flags; } macho_segment_cmd_64;
EFI_STATUS load_macho64(EFI_SYSTEM_TABLE *st, const void *image, UINTN size, void **entry,
                        kernel_segment_t *segs, uint32_t *segc) {
    const uint8_t *buf = (const uint8_t*)image;
    const macho64_hdr_t *hdr = (const macho64_hdr_t*)buf;
    if (hdr->magic != MH_MAGIC_64) return EFI_LOAD_ERROR;
    uint64_t entry_offset = 0;
    int found_entry = 0, count = 0;
    const uint8_t *cmdptr = buf + sizeof(macho64_hdr_t);
    for (uint32_t i = 0; i < hdr->ncmds; ++i) {
        const macho_loadcmd_t *cmd = (const macho_loadcmd_t *)cmdptr;
        if (cmd->cmd == 0x80000028 && cmd->cmdsize >= 24) { // LC_MAIN
            struct { uint32_t cmd, cmdsize; uint64_t entryoff, stacksize; } *main = (void*)cmdptr;
            entry_offset = main->entryoff;
            found_entry = 1;
        }
        cmdptr += cmd->cmdsize;
    }
    cmdptr = buf + sizeof(macho64_hdr_t);
    for (uint32_t i = 0; i < hdr->ncmds; ++i) {
        const macho_loadcmd_t *cmd = (const macho_loadcmd_t *)cmdptr;
        if (cmd->cmd == 0x19 && cmd->cmdsize >= sizeof(macho_segment_cmd_64)) { // LC_SEGMENT_64
            const macho_segment_cmd_64 *seg = (const macho_segment_cmd_64 *)cmd;
            EFI_PHYSICAL_ADDRESS seg_addr = seg->vmaddr;
            UINTN seg_pages = (seg->vmsize + 0xFFF) / 0x1000;
            EFI_STATUS s = st->BootServices->AllocatePages(EFI_ALLOCATE_ADDRESS, EfiLoaderData, seg_pages, &seg_addr);
            if (EFI_ERROR(s)) return s;
            if (seg->filesize > 0)
                memcpy((void *)(uintptr_t)seg_addr, buf + seg->fileoff, seg->filesize);
            if (seg->vmsize > seg->filesize)
                memset((void *)(uintptr_t)(seg_addr + seg->filesize), 0, seg->vmsize - seg->filesize);

            print_ascii(st, "[O2] MACHO LOAD: vmaddr="); print_hex(st, seg->vmaddr);
            print_ascii(st, " paddr="); print_hex(st, seg_addr);
            print_ascii(st, " filesz="); print_hex(st, seg->filesize);
            print_ascii(st, " memsz="); print_hex(st, seg->vmsize);
            print_ascii(st, " name=");
            for (int n=0; n<16 && seg->segname[n]; n++) {
                char c[2] = {seg->segname[n], 0};
                print_ascii(st, c);
            }
            print_ascii(st, "\r\n");

            if (segs && segc && count < MAX_KERNEL_SEGMENTS) {
                segs[count].vaddr  = seg->vmaddr;
                segs[count].paddr  = seg_addr;
                segs[count].filesz = seg->filesize;
                segs[count].memsz  = seg->vmsize;
                segs[count].flags  = seg->initprot;
                memcpy(segs[count].name, seg->segname, 16); segs[count].name[16]=0;
                ++count;
            }
        }
        cmdptr += cmd->cmdsize;
    }
    if (segc) *segc = count;
    g_kernel_base = segs[0].paddr;
    g_kernel_size = 0; for (uint32_t i=0; i<count; i++) g_kernel_size += segs[i].memsz;
    *entry = (void *)(uintptr_t)(found_entry ? entry_offset : segs[0].paddr);
    return EFI_SUCCESS;
}

// --- Mach-O FAT ---
typedef struct { uint32_t magic, nfat_arch; } fat_header_t;
typedef struct { uint32_t cputype,cpusubtype,offset,size,align; } fat_arch_t;
EFI_STATUS load_macho_fat(EFI_SYSTEM_TABLE *st, const void *image, UINTN size, void **entry,
                          kernel_segment_t *segs, uint32_t *segc) {
    const fat_header_t *fat = (const fat_header_t*)image;
    if (fat->magic != FAT_MAGIC) return EFI_LOAD_ERROR;
    const fat_arch_t *archs = (const fat_arch_t *)(fat + 1);
    for (uint32_t i = 0; i < fat->nfat_arch; ++i) {
        if (archs[i].cputype == 0x01000007 || archs[i].cputype == 0x0100000C) { // x86_64 or ARM64
            const uint8_t *thin = (const uint8_t *)image + archs[i].offset;
            UINTN thin_size = archs[i].size;
            return load_macho64(st, thin, thin_size, entry, segs, segc);
        }
    }
    return EFI_LOAD_ERROR;
}

// --- MAIN ENTRY ---
EFI_STATUS EFIAPI efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    print_ascii(SystemTable, "\r\n[O2] Universal UEFI bootloader\r\n");

    EFI_LOADED_IMAGE_PROTOCOL *loaded;
    EFI_STATUS status = SystemTable->BootServices->HandleProtocol(ImageHandle,
        (EFI_GUID*)&gEfiLoadedImageProtocolGuid, (void**)&loaded);
    if (EFI_ERROR(status)) { print_ascii(SystemTable, "LoadProtocol failed\r\n"); return status; }
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs;
    status = SystemTable->BootServices->HandleProtocol(loaded->DeviceHandle,
        (EFI_GUID*)&gEfiSimpleFileSystemProtocolGuid, (void**)&fs);
    if (EFI_ERROR(status)) { print_ascii(SystemTable, "FS Protocol failed\r\n"); return status; }
    EFI_FILE_PROTOCOL *root;
    status = fs->OpenVolume(fs, &root);
    if (EFI_ERROR(status)) { print_ascii(SystemTable, "OpenVolume failed\r\n"); return status; }

    // --- Load kernel ---
    void *kernel_file = NULL; UINTN kernel_size = 0;
    status = load_file(SystemTable, root, KERNEL_NAME, &kernel_file, &kernel_size);
    if (EFI_ERROR(status)) { print_ascii(SystemTable, "Kernel not found\r\n"); return status; }
    kernel_type_t ktype = detect_kernel_type((const uint8_t*)kernel_file, kernel_size);
    print_ascii(SystemTable, "[O2] Kernel type: ");
    if (ktype == KERNEL_TYPE_ELF64) print_ascii(SystemTable, "ELF64\r\n");
    else if (ktype == KERNEL_TYPE_MACHO64) print_ascii(SystemTable, "Mach-O 64\r\n");
    else if (ktype == KERNEL_TYPE_MACHO_FAT) print_ascii(SystemTable, "FAT Mach-O\r\n");
    else if (ktype == KERNEL_TYPE_PECOFF) print_ascii(SystemTable, "PE/COFF\r\n");
    else if (ktype == KERNEL_TYPE_FLAT) print_ascii(SystemTable, "Flat bin\r\n");
    else print_ascii(SystemTable, "Unknown\r\n");

    void *entry = NULL;
    kernel_segment_t kernel_segments[MAX_KERNEL_SEGMENTS];
    uint32_t kernel_segment_count = 0;
    if (ktype == KERNEL_TYPE_ELF64)
        status = load_elf64(SystemTable, kernel_file, kernel_size, &entry, kernel_segments, &kernel_segment_count);
    else if (ktype == KERNEL_TYPE_MACHO64)
        status = load_macho64(SystemTable, kernel_file, kernel_size, &entry, kernel_segments, &kernel_segment_count);
    else if (ktype == KERNEL_TYPE_MACHO_FAT)
        status = load_macho_fat(SystemTable, kernel_file, kernel_size, &entry, kernel_segments, &kernel_segment_count);
    else if (ktype == KERNEL_TYPE_PECOFF)
        status = load_pe_coff(SystemTable, kernel_file, kernel_size, &entry, kernel_segments, &kernel_segment_count);
    else if (ktype == KERNEL_TYPE_FLAT)
        status = load_flat(SystemTable, kernel_file, kernel_size, &entry, kernel_segments, &kernel_segment_count);

    if (EFI_ERROR(status)) { print_ascii(SystemTable, "Kernel load error\r\n"); return status; }
    print_ascii(SystemTable, "[O2] Kernel entry: "); print_hex(SystemTable, (uint64_t)(uintptr_t)entry); print_ascii(SystemTable, "\r\n");
    SystemTable->BootServices->FreePool(kernel_file);

    // --- Build bootinfo ---
    bootinfo_t *bi;
    EFI_PHYSICAL_ADDRESS bi_phys = 0;
    UINTN bi_pages = (sizeof(bootinfo_t) + 0xFFF) / 0x1000;
    status = SystemTable->BootServices->AllocatePages(EFI_ALLOCATE_ANY_PAGES, EfiLoaderData, bi_pages, &bi_phys);
    if (EFI_ERROR(status)) { print_ascii(SystemTable, "bootinfo alloc fail\r\n"); return status; }
    bi = (bootinfo_t *)(uintptr_t)bi_phys;
    memset(bi, 0, bi_pages * 0x1000);
    bi->magic = BOOTINFO_MAGIC_UEFI;
    bi->size = sizeof(*bi);
    memcpy(bi->kernel_segments, kernel_segments, sizeof(kernel_segment_t)*kernel_segment_count);
    bi->kernel_segment_count = kernel_segment_count;
    bi->kernel_load_base = g_kernel_base;
    bi->kernel_load_size = g_kernel_size;
    bi->kernel_entry = entry;

    log_bootinfo(SystemTable, bi);

    // (You'd finish with ExitBootServices and call the kernel entry as usual)
    return EFI_SUCCESS;
}
