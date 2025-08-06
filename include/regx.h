#ifndef NITROS_REGX_H
#define NITROS_REGX_H

#include <stdint.h>
#include <stddef.h>

/* Maximum entries in RegX registry.  The value is small for early boot but
 * can be increased as the system grows. */
#define REGX_MAX_ENTRIES 256

/* Entry types handled by the registry.  Devices form a hierarchy through the
 * parent field while agents and services are flat. */
typedef enum {
    REGX_TYPE_AGENT,
    REGX_TYPE_DRIVER,
    REGX_TYPE_FILESYSTEM,
    REGX_TYPE_DEVICE,
    REGX_TYPE_SERVICE
} regx_type_t;

/* Runtime state flags for each entry.  All updates are versioned through the
 * generation counter to guarantee atomicity and hot‑update safety. */
#define REGX_STATE_ACTIVE   0x1
#define REGX_STATE_PAUSED   0x2
#define REGX_STATE_ERROR    0x4

/* Manifest describing a component.  Strings are fixed size for simplicity;
 * real implementations may use dynamic allocation or CBOR/JSON blobs. */
typedef struct {
    char name[64];
    regx_type_t type;
    char version[16];
    char abi[16];
    char capabilities[256];
    char permissions[256];
    char dependencies[256];
} regx_manifest_t;

/* Registry entry.  The manifest is immutable while the runtime state can be
 * updated atomically.  parent_id links devices/buses into a tree; agents and
 * services use parent_id = 0. */
typedef struct {
    uint64_t id;            /* unique monotonically increasing ID */
    uint64_t parent_id;     /* parent for hierarchical resources */
    uint64_t state;         /* REGX_STATE_* flags */
    uint64_t generation;    /* incremented on every update */
    regx_manifest_t manifest;
    const void *signature;  /* pointer to auth signature */
    void *priv;             /* implementation specific pointer */
} regx_entry_t;

/* Selector used for queries/enumeration.  Any field may be zero/NULL to act as
 * a wildcard. */
typedef struct {
    regx_type_t type;       /* filter by type */
    const char *capability; /* required capability */
    uint64_t parent_id;     /* restrict to children of this ID */
} regx_selector_t;

/* Kernel‑internal API.  All functions return 0 on success or negative errno. */
int regx_register(const regx_entry_t *entry);      /* atomic add */
int regx_unregister(uint64_t id);                  /* atomic remove */
int regx_update(uint64_t id, const regx_entry_t *delta); /* atomic update */
const regx_entry_t *regx_query(uint64_t id);       /* lookup by ID */
size_t regx_enumerate(const regx_selector_t *sel,
                      regx_entry_t *out, size_t max); /* filtered list */

#endif /* NITROS_REGX_H */
