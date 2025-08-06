#ifndef NOS_ALC_H
#define NOS_ALC_H

#include <stdint.h>

#define ALC_MAGIC 0x30434C41u /* 'ALC0' */

/* Types of objects stored in the cache */
#define ALC_TYPE_MODULE   1
#define ALC_TYPE_LIBRARY  2
#define ALC_TYPE_MANIFEST 3
#define ALC_TYPE_WASM     4

/* Cache header placed at the start of every ALC file */
typedef struct {
    uint32_t magic;       /* must be ALC_MAGIC */
    uint16_t version;     /* format version */
    uint16_t reserved;    /* future use */
    char     arch[16];    /* target architecture */
    char     abi[16];     /* ABI tag */
    uint32_t entry_count; /* number of entries */
    uint32_t index_offset;/* offset to entry array */
    uint32_t sig_offset;  /* offset to cache signature */
} __attribute__((packed)) alc_header_t;

/* Index record for each cached object */
typedef struct {
    uint32_t type;        /* ALC_TYPE_* */
    uint32_t flags;       /* hot reload, prelinked, etc. */
    uint64_t blob_offset; /* location of code/data blob */
    uint64_t blob_size;   /* size of blob */
    uint64_t manifest_off;/* offset to embedded manifest */
    uint64_t manifest_sz; /* size of manifest */
    uint8_t  hash[32];    /* SHA-256 of blob+manifest */
    uint8_t  sig[64];     /* signature over hash */
    char     name[64];
    char     version[16];
    char     abi[16];
} __attribute__((packed)) alc_entry_t;

/* Usage metrics kept outside the immutable cache image */
typedef struct {
    uint64_t hits;        /* number of mappings */
    uint64_t last_used;   /* timestamp of last use */
    uint8_t  locked;      /* pinned entry */
    uint8_t  reserved[7];
} alc_usage_t;

/* In-memory representation of a mapped cache */
typedef struct {
    const alc_header_t *hdr;
    const alc_entry_t  *entries;
    alc_usage_t        *usage; /* array of entry_count usage records */
} alc_cache_t;

/* Lookup an entry by name/version/ABI */
const alc_entry_t *alc_find(const alc_cache_t *cache,
                            const char *name,
                            const char *version,
                            const char *abi);

/* Map an entry into memory and perform relocations */
void *alc_map(const alc_entry_t *entry);

/* Validate hash and signature of an entry */
int alc_validate(const alc_entry_t *entry);

#endif /* NOS_ALC_H */
