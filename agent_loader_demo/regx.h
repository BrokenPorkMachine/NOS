#ifndef REGX_H
#define REGX_H

#include <stddef.h>
#include <stdint.h>

/* Simple RegX registry holding agent manifests. */

typedef struct {
    char name[64];
    char type[32];
    char version[32];
    char abi[32];
    char capabilities[128];
    char entry[64];
} regx_manifest_t;

typedef struct {
    uint64_t id;
    uint64_t parent_id;
    regx_manifest_t manifest;
} regx_entry_t;

typedef struct {
    const char *type; /* optional filter by type */
} regx_selector_t;

uint64_t regx_register(const regx_manifest_t *manifest, uint64_t parent_id);
const regx_entry_t *regx_query(uint64_t id);
size_t regx_enumerate(const regx_selector_t *sel, regx_entry_t *out, size_t max);

#endif /* REGX_H */
