#pragma once
#include <stdint.h>
#define REGX_MAX_ENTRIES 128

typedef struct {
    char name[32];
    int  type;                // e.g. 1=device, 2=driver, 3=agent
    char version[16];
    char abi[16];
    char capabilities[64];
} regx_manifest_t;

typedef struct regx_entry {
    uint64_t id;
    uint64_t parent_id;      // 0=root
    regx_manifest_t manifest;
    // room for extensions
} regx_entry_t;

typedef struct {
    // Selector/filter: can be extended (e.g. by type, capability, parent, etc)
    int type;
    uint64_t parent_id;
    char name_prefix[16];
} regx_selector_t;
