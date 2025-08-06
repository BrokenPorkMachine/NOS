#ifndef NOS_MACHO2_H
#define NOS_MACHO2_H

#include <stdint.h>

/* Mach-O2 custom load command for manifest information */
#define LC_MACHO2INFO 0x80000035

/* Magic for 64-bit little endian Mach-O */
#ifndef MH_MAGIC_64
#define MH_MAGIC_64 0xfeedfacf
#endif

struct mach_header_64 {
    uint32_t magic;
    int32_t  cputype;
    int32_t  cpusubtype;
    uint32_t filetype;
    uint32_t ncmds;
    uint32_t sizeofcmds;
    uint32_t flags;
    uint32_t reserved;
};

struct load_command {
    uint32_t cmd;
    uint32_t cmdsize;
};

/* Load command carrying manifest location */
struct macho2_info_command {
    uint32_t cmd;        /* LC_MACHO2INFO */
    uint32_t cmdsize;    /* sizeof(struct macho2_info_command) */
    uint64_t manifest_offset; /* file offset of embedded manifest */
    uint64_t manifest_size;   /* size of manifest in bytes */
    uint64_t reserved;        /* for future use */
};

struct macho2_resource {
    char     name[32];   /* arbitrary identifier */
    uint64_t offset;     /* file offset */
    uint64_t size;       /* size in bytes */
};

#define MACHO2_MAX_PRIVS 8
#define MACHO2_MAX_RES   16

struct macho2_manifest {
    char name[64];
    char type[16];
    char version[16];
    char entry[64];
    char privileges[MACHO2_MAX_PRIVS][32];
    uint32_t privilege_count;
    struct macho2_resource resources[MACHO2_MAX_RES];
    uint32_t resource_count;
};

int macho2_load_manifest(const char *path, struct macho2_manifest *out);
int macho2_extract_resource(const char *path, const struct macho2_resource *res,
                            void **buf, uint64_t *size);

#endif /* NOS_MACHO2_H */
