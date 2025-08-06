#pragma once
#include <stdint.h>
#define REGX_MAX_ENTRIES 256

typedef struct {
    char name[32];
    int  type;                // 1=device, 2=driver, 3=agent, 4=service, 5=bus
    char version[16];
    char abi[16];
    char capabilities[64];
} regx_manifest_t;

typedef struct regx_entry {
    uint64_t id;
    uint64_t parent_id;      // 0=root
    regx_manifest_t manifest;
} regx_entry_t;

typedef struct {
    int type;                // 0=any
    uint64_t parent_id;      // 0=any
    char name_prefix[16];    // ""=any
} regx_selector_t;
