#pragma once
#include <stdint.h>
#include <stddef.h>
#define REGX_MAX_ENTRIES 128

// Registry entry type identifiers
enum {
    REGX_TYPE_DEVICE = 1,
    REGX_TYPE_DRIVER,
    REGX_TYPE_FILESYSTEM,
    REGX_TYPE_AGENT,
    REGX_TYPE_SERVICE,
};

// Runtime state flags
enum {
    REGX_STATE_INACTIVE = 0,
    REGX_STATE_ACTIVE,
    REGX_STATE_PAUSED,
    REGX_STATE_ERROR,
};

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
    int state;               // runtime state of the entry
    uint32_t generation;     // incremented on every update
    const void *signature;   // optional authentication data
} regx_entry_t;

typedef struct {
    // Selector/filter: can be extended (e.g. by type, capability, parent, etc)
    int type;
    uint64_t parent_id;
    char name_prefix[16];
} regx_selector_t;

// Registry API
uint64_t regx_register(const regx_entry_t *entry);
int      regx_unregister(uint64_t id);
int      regx_update(uint64_t id, const regx_entry_t *delta);
const regx_entry_t *regx_query(uint64_t id);
size_t   regx_enumerate(const regx_selector_t *sel, regx_entry_t *out, size_t max);
void     regx_tree(uint64_t parent, int level);
