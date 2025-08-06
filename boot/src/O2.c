#include "efi.h"
#include "bootinfo.h"
#include "../../include/nosm.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

static void *memcpy(void *dst, const void *src, size_t n) {
    uint8_t *d = dst; const uint8_t *s = src; while (n--) *d++ = *s++; return dst;
}
static void *memset(void *dst, int c, size_t n) {
    uint8_t *d = dst; while (n--) *d++ = (uint8_t)c; return dst;
}
static int memcmp(const void *a, const void *b, size_t n) {
    const uint8_t *x=a, *y=b; while (n--) { if (*x!=*y) return *x-*y; x++; y++; } return 0;
}

// Simple SHA-256 implementation -------------------------------------------
typedef struct {
    uint32_t state[8];
    uint64_t bitlen;
    uint8_t  data[64];
    uint32_t datalen;
} sha256_ctx;

static const uint32_t k256[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};

static uint32_t rotr32(uint32_t x, uint32_t n) {
    return (x >> n) | (x << (32 - n));
}

static void sha256_transform(sha256_ctx *ctx, const uint8_t data[64]) {
    uint32_t m[64];
    for (int i = 0; i < 16; ++i) {
        m[i] = (uint32_t)data[i*4] << 24 |
               (uint32_t)data[i*4+1] << 16 |
               (uint32_t)data[i*4+2] << 8 |
               (uint32_t)data[i*4+3];
    }
    for (int i = 16; i < 64; ++i) {
        uint32_t s0 = rotr32(m[i-15],7) ^ rotr32(m[i-15],18) ^ (m[i-15] >> 3);
        uint32_t s1 = rotr32(m[i-2],17) ^ rotr32(m[i-2],19) ^ (m[i-2] >> 10);
        m[i] = m[i-16] + s0 + m[i-7] + s1;
    }

    uint32_t a=ctx->state[0], b=ctx->state[1], c=ctx->state[2], d=ctx->state[3];
    uint32_t e=ctx->state[4], f=ctx->state[5], g=ctx->state[6], h=ctx->state[7];
    for (int i = 0; i < 64; ++i) {
        uint32_t S1 = rotr32(e,6) ^ rotr32(e,11) ^ rotr32(e,25);
        uint32_t ch = (e & f) ^ (~e & g);
        uint32_t temp1 = h + S1 + ch + k256[i] + m[i];
        uint32_t S0 = rotr32(a,2) ^ rotr32(a,13) ^ rotr32(a,22);
        uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        uint32_t temp2 = S0 + maj;
        h = g;
        g = f;
        f = e;
        e = d + temp1;
        d = c;
        c = b;
        b = a;
        a = temp1 + temp2;
    }

    ctx->state[0] += a;
    ctx->state[1] += b;
    ctx->state[2] += c;
    ctx->state[3] += d;
    ctx->state[4] += e;
    ctx->state[5] += f;
    ctx->state[6] += g;
    ctx->state[7] += h;
}

static void sha256_init(sha256_ctx *ctx) {
    ctx->datalen = 0;
    ctx->bitlen = 0;
    ctx->state[0]=0x6a09e667; ctx->state[1]=0xbb67ae85;
    ctx->state[2]=0x3c6ef372; ctx->state[3]=0xa54ff53a;
    ctx->state[4]=0x510e527f; ctx->state[5]=0x9b05688c;
    ctx->state[6]=0x1f83d9ab; ctx->state[7]=0x5be0cd19;
}

static void sha256_update(sha256_ctx *ctx, const uint8_t *data, size_t len) {
    for (size_t i = 0; i < len; ++i) {
        ctx->data[ctx->datalen++] = data[i];
        if (ctx->datalen == 64) {
            sha256_transform(ctx, ctx->data);
            ctx->bitlen += 512;
            ctx->datalen = 0;
        }
    }
}

static void sha256_final(sha256_ctx *ctx, uint8_t hash[32]) {
    uint32_t i = ctx->datalen;

    ctx->data[i++] = 0x80;
    if (i > 56) {
        while (i < 64) ctx->data[i++] = 0;
        sha256_transform(ctx, ctx->data);
        i = 0;
    }
    while (i < 56) ctx->data[i++] = 0;
    ctx->bitlen += ctx->datalen * 8;
    ctx->data[63] = ctx->bitlen;
    ctx->data[62] = ctx->bitlen >> 8;
    ctx->data[61] = ctx->bitlen >> 16;
    ctx->data[60] = ctx->bitlen >> 24;
    ctx->data[59] = ctx->bitlen >> 32;
    ctx->data[58] = ctx->bitlen >> 40;
    ctx->data[57] = ctx->bitlen >> 48;
    ctx->data[56] = ctx->bitlen >> 56;
    sha256_transform(ctx, ctx->data);

    for (i = 0; i < 4; ++i)
        for (int j = 0; j < 8; ++j)
            hash[j*4 + i] = (ctx->state[j] >> (24 - i * 8)) & 0xff;
}

static void compute_sha256(const uint8_t *data, size_t len, uint8_t out[32]) {
    sha256_ctx ctx;
    sha256_init(&ctx);
    sha256_update(&ctx, data, len);
    sha256_final(&ctx, out);
}

// Utility functions --------------------------------------------------------
static void print(EFI_SYSTEM_TABLE *st, const CHAR16 *msg) {
    st->ConOut->OutputString(st->ConOut, msg);
}

static void print_ascii(EFI_SYSTEM_TABLE *st, const char *s) {
    CHAR16 buf[128];
    size_t i;
    for (i = 0; s[i] && i < 127; ++i) buf[i] = (CHAR16)s[i];
    buf[i] = 0;
    print(st, buf);
}

static size_t strlen16(const CHAR16 *s) {
    size_t i = 0; while (s[i]) i++; return i; }

static bool ends_with16(const CHAR16 *s, const CHAR16 *suffix) {
    size_t ls = strlen16(s), lf = strlen16(suffix);
    if (ls < lf) return false;
    return !memcmp(s + ls - lf, suffix, lf * sizeof(CHAR16));
}

static void to_ascii(const CHAR16 *src, char *dst, size_t max) {
    size_t i;
    for (i=0; src[i] && i<max-1; ++i)
        dst[i] = (char)(src[i] & 0xFF);
    dst[i] = 0;
}

static EFI_STATUS load_file(EFI_SYSTEM_TABLE *st, EFI_FILE_PROTOCOL *root,
                            const CHAR16 *path, void **buf, UINTN *size) {
    EFI_FILE_PROTOCOL *file;
    EFI_STATUS status = root->Open(root, &file, (CHAR16*)path, EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(status)) return status;

    UINTN infoSize = sizeof(EFI_FILE_INFO) + 256;
    EFI_FILE_INFO *info;
    status = st->BootServices->AllocatePool(EfiLoaderData, infoSize, (void**)&info);
    if (EFI_ERROR(status)) { file->Close(file); return status; }
    status = file->GetInfo(file, (EFI_GUID*)&gEfiFileInfoGuid, &infoSize, info);
    if (EFI_ERROR(status)) { st->BootServices->FreePool(info); file->Close(file); return status; }

    *size = info->FileSize;
    st->BootServices->FreePool(info);
    status = st->BootServices->AllocatePool(EfiLoaderData, *size, buf);
    if (EFI_ERROR(status)) { file->Close(file); return status; }
    status = file->Read(file, size, *buf);
    file->Close(file);
    return status;
}

static EFI_STATUS load_signature(EFI_SYSTEM_TABLE *st, EFI_FILE_PROTOCOL *root,
                                 const CHAR16 *path, uint8_t sig[32]) {
    void *buf; UINTN sz;
    EFI_STATUS status = load_file(st, root, path, &buf, &sz);
    if (EFI_ERROR(status)) return status;
    if (sz != 32) { st->BootServices->FreePool(buf); return EFI_SECURITY_VIOLATION; }
    memcpy(sig, buf, 32);
    st->BootServices->FreePool(buf);
    return EFI_SUCCESS;
}

static EFI_STATUS verify_signature(EFI_SYSTEM_TABLE *st, EFI_FILE_PROTOCOL *root,
                                   const CHAR16 *sigpath,
                                   const uint8_t *data, size_t len,
                                   uint8_t out_hash[32]) {
    uint8_t expected[32];
    EFI_STATUS status = load_signature(st, root, sigpath, expected);
    if (EFI_ERROR(status)) return status;
    compute_sha256(data, len, out_hash);
    if (memcmp(expected, out_hash, 32) != 0)
        return EFI_SECURITY_VIOLATION;
    return EFI_SUCCESS;
}

static bool guid_equal(const EFI_GUID *a, const EFI_GUID *b) {
    return !memcmp(a, b, sizeof(EFI_GUID));
}

// Bootloader main ----------------------------------------------------------
EFI_STATUS EFIAPI efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    print_ascii(SystemTable, "[O2] Booting NitrOS...\r\n");

    // Secure Boot check
    UINT8 secure = 0; UINTN ssz = sizeof(secure);
    bool secure_boot = false;
    EFI_STATUS status = SystemTable->RuntimeServices->GetVariable(
        L"SecureBoot", (EFI_GUID*)&gEfiGlobalVariableGuid, NULL, &ssz, &secure);
    if (EFI_ERROR(status) || secure == 0) {
        print_ascii(SystemTable, "[O2] Secure Boot disabled\r\n");
    } else {
        print_ascii(SystemTable, "[O2] Secure Boot enabled\r\n");
        secure_boot = true;
    }

    EFI_LOADED_IMAGE_PROTOCOL *loaded;
    status = SystemTable->BootServices->HandleProtocol(ImageHandle,
        (EFI_GUID*)&gEfiLoadedImageProtocolGuid, (void**)&loaded);
    if (EFI_ERROR(status)) return status;

    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs;
    status = SystemTable->BootServices->HandleProtocol(loaded->DeviceHandle,
        (EFI_GUID*)&gEfiSimpleFileSystemProtocolGuid, (void**)&fs);
    if (EFI_ERROR(status)) return status;

    EFI_FILE_PROTOCOL *root;
    status = fs->OpenVolume(fs, &root);
    if (EFI_ERROR(status)) return status;

    bootinfo_t *bi;
    status = SystemTable->BootServices->AllocatePool(EfiLoaderData, sizeof(bootinfo_t), (void**)&bi);
    if (EFI_ERROR(status)) return status;
    memset(bi, 0, sizeof(*bi));
    bi->magic = BOOTINFO_MAGIC_UEFI;
    bi->size = sizeof(*bi);
    bi->bootloader_name = "O2 UEFI";

    // Load kernel -------------------------------------------------------
    void *tmp; UINTN ksize;
    status = load_file(SystemTable, root, L"\\kernel.bin", &tmp, &ksize);
    if (EFI_ERROR(status)) return status;

    EFI_PHYSICAL_ADDRESS kphys = 0x100000; // load kernel at 1MB
    UINTN pages = (ksize + 0xFFF) / 0x1000;
    status = SystemTable->BootServices->AllocatePages(AllocateAddress,
                                                      EfiLoaderData,
                                                      pages, &kphys);
    if (EFI_ERROR(status)) {
        SystemTable->BootServices->FreePool(tmp);
        return status;
    }

    void *kernel = (void*)(uintptr_t)kphys;
    memcpy(kernel, tmp, ksize);
    SystemTable->BootServices->FreePool(tmp);

    uint8_t ksig[32];
    if (secure_boot) {
        status = verify_signature(SystemTable, root, L"\\kernel.bin.sig",
                                  kernel, ksize, ksig);
        if (EFI_ERROR(status)) return status;
    } else {
        compute_sha256(kernel, ksize, ksig);
    }
    bi->kernel_entry = kernel;
    bi->kernel_segs.file_base = (uint64_t)kphys;
    bi->kernel_segs.file_size = ksize;

    // Load modules ------------------------------------------------------
    EFI_FILE_INFO *info;
    UINTN infoSize = sizeof(EFI_FILE_INFO) + 256;
    status = SystemTable->BootServices->AllocatePool(EfiLoaderData, infoSize, (void**)&info);
    if (EFI_ERROR(status)) return status;
    while (true) {
        UINTN sz = infoSize;
        status = root->Read(root, &sz, info);
        if (EFI_ERROR(status) || sz == 0) break;
        if (info->Attribute & EFI_FILE_DIRECTORY) continue;
        if (!ends_with16(info->FileName, L".nosm")) continue;
        void *mod; UINTN msz;
        status = load_file(SystemTable, root, info->FileName, &mod, &msz);
        if (EFI_ERROR(status)) continue;
        CHAR16 sigpath[128];
        size_t flen = strlen16(info->FileName);
        memcpy(sigpath, info->FileName, flen * sizeof(CHAR16));
        sigpath[flen] = L'.';
        sigpath[flen+1] = L's';
        sigpath[flen+2] = L'i';
        sigpath[flen+3] = L'g';
        sigpath[flen+4] = 0;
        uint8_t hash[32];
        if (secure_boot) {
            status = verify_signature(SystemTable, root, sigpath, mod, msz, hash);
            if (EFI_ERROR(status)) { SystemTable->BootServices->FreePool(mod); continue; }
        } else {
            compute_sha256(mod, msz, hash);
        }
        bootinfo_module_t *m = &bi->modules[bi->module_count];
        m->base = (uint64_t)(uintptr_t)mod;
        m->size = msz;
        to_ascii(info->FileName, m->name, sizeof(m->name));
        memcpy(m->sha256, hash, 32);
        const nosm_header_t *nh = (const nosm_header_t *)mod;
        if (nh->magic == NOSM_MAGIC &&
            nh->manifest_offset + nh->manifest_size <= msz) {
            m->manifest_addr = (uint64_t)(uintptr_t)((uint8_t*)mod + nh->manifest_offset);
            m->manifest_size = nh->manifest_size;
        }
        bi->module_count++;
        if (bi->module_count >= BOOTINFO_MAX_MODULES) break;
    }
    SystemTable->BootServices->FreePool(info);

    // Graphics ---------------------------------------------------------
    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop;
    status = SystemTable->BootServices->LocateProtocol(
        (EFI_GUID*)&gEfiGraphicsOutputProtocolGuid, NULL, (void**)&gop);
    if (!EFI_ERROR(status)) {
        bootinfo_framebuffer_t *fb;
        status = SystemTable->BootServices->AllocatePool(EfiLoaderData, sizeof(*fb), (void**)&fb);
        if (!EFI_ERROR(status)) {
            fb->address = gop->Mode->FrameBufferBase;
            fb->width   = gop->Mode->Info->HorizontalResolution;
            fb->height  = gop->Mode->Info->VerticalResolution;
            fb->pitch   = gop->Mode->Info->PixelsPerScanLine * 4;
            fb->bpp     = 32;
            fb->type    = gop->Mode->Info->PixelFormat;
            fb->reserved = 0;
            bi->framebuffer = fb;
        }
    }

    // ACPI -------------------------------------------------------------
    EFI_CONFIGURATION_TABLE *ct = (EFI_CONFIGURATION_TABLE*)SystemTable->ConfigurationTable;
    for (UINTN i = 0; i < SystemTable->NumberOfTableEntries; ++i) {
        if (guid_equal(&ct[i].VendorGuid, &gEfiAcpi20TableGuid) ||
            guid_equal(&ct[i].VendorGuid, &gEfiAcpi10TableGuid)) {
            bi->acpi_rsdp = (uint64_t)(uintptr_t)ct[i].VendorTable;
            break;
        }
    }

    // Memory map -------------------------------------------------------
    UINTN mmapSize = 0, mapKey = 0, descSize = 0; UINT32 descVer = 0;
    SystemTable->BootServices->GetMemoryMap(&mmapSize, NULL, &mapKey, &descSize, &descVer);
    mmapSize += descSize * 2;
    status = SystemTable->BootServices->AllocatePool(EfiLoaderData, mmapSize, (void**)&bi->mmap);
    if (EFI_ERROR(status)) return status;
    status = SystemTable->BootServices->GetMemoryMap(&mmapSize, bi->mmap, &mapKey, &descSize, &descVer);
    if (EFI_ERROR(status)) return status;
    bi->mmap_entries = mmapSize / descSize;
    bi->reserved[0] = bi->mmap_entries;

    // Exit boot services -----------------------------------------------
    status = SystemTable->BootServices->ExitBootServices(ImageHandle, mapKey);
    if (EFI_ERROR(status)) return status;

    // Jump to kernel ---------------------------------------------------
    void (*kernel_entry)(bootinfo_t*) = (void(*)(bootinfo_t*))bi->kernel_entry;
    kernel_entry(bi);
    return EFI_SUCCESS;
}

