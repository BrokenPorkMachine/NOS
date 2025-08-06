#pragma once
#include <stdint.h>
#include <stddef.h>

#define REGX_MAX_ENTRIES 128

// Each manifest describes a driver/agent/device
typedef struct {
    char name[32];
    int  type;         // 1=device, 2=driver, 3=agent, ...
    char version[16];
    char abi[16];
    char capabilities[64];
} regx_manifest_t;

typedef struct regx_entry {
    uint64_t id;
    uint64_t parent_id;
    regx_manifest_t manifest;
} regx_entry_t;

typedef struct {
    int type;
    uint64_t parent_id;
    char name_prefix[16];
} regx_selector_t;

uint64_t regx_register(const regx_manifest_t *manifest, uint64_t parent_id);
const regx_entry_t *regx_query(uint64_t id);
size_t regx_enumerate(const regx_selector_t *sel, regx_entry_t *out, size_t max);
void regx_reset(void);

