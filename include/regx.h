#pragma once
#include <stddef.h>
#include <stdint.h>

#define REGX_MAX_ENTRIES 256

// regx.h
typedef enum {
    REGX_TYPE_UNSPEC = 0,
    REGX_TYPE_DEVICE = 1,
    REGX_TYPE_DRIVER = 2,
    REGX_TYPE_AGENT  = 3,
    REGX_TYPE_SERVICE = 4,
    REGX_TYPE_BUS = 5,
    REGX_TYPE_FILESYSTEM = 6,
} regx_type_t;

/* Registry entry types */
#define REGX_TYPE_ANY        0
#define REGX_TYPE_DEVICE     1
#define REGX_TYPE_DRIVER     2
#define REGX_TYPE_AGENT      3
#define REGX_TYPE_SERVICE    4
#define REGX_TYPE_BUS        5
#define REGX_TYPE_FILESYSTEM 6

typedef struct {
    char name[32];
    int  type;                /* one of REGX_TYPE_* */
    char version[16];
    char abi[16];
    char capabilities[64];
} regx_manifest_t;

typedef struct regx_entry {
    uint64_t id;
    uint64_t parent_id;      /* 0=root */
    regx_manifest_t manifest;
} regx_entry_t;

typedef struct {
    int type;                /* REGX_TYPE_ANY to match any */
    uint64_t parent_id;      /* 0=any */
    char name_prefix[16];    /* ""=any */
} regx_selector_t;

uint64_t regx_register(const regx_manifest_t *m, uint64_t parent_id);
int      regx_unregister(uint64_t id);
const regx_entry_t *regx_query(uint64_t id);
size_t   regx_enumerate(const regx_selector_t *sel, regx_entry_t *out, size_t max);
